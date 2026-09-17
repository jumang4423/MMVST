(
// Stereo-linked dynamics processor with normalized 0..1 controls.
// Missing controls use an aggressive punch-focused performance preset.
var effectName = \dirt_mmd;
var numChannels = ~dirt.numChannels;
var factoryMid = 64 / 127;

SynthDef(effectName.asString ++ numChannels, {
    |out,
    mmdAtk = 0.503937, mmdRel = 0.157480, mmdThrs = 0.692913,
    mmdMix = 1, mmdRat = 0.881890, mmdGain = 0.377953, mmdRms = 0.125984,
    mmdInp = 0.503937|

    var dry = In.ar(out, numChannels);
    var left = dry[0];
    var right = if(numChannels > 1, { dry[1] }, { dry[0] });
    var stereoDry = [left, right];
    var atk, rel, thresholdDb, ratio, makeupDb, rmsTime, inputGain;
    var input, peakLevel, rmsLevel, detector, levelDb, overDb;
    var reductionTargetDb, reductionDb, compressed, clipped, mixed;

    mmdAtk = Lag.kr(mmdAtk.clip(0, 1), 0.03);
    mmdRel = Lag.kr(mmdRel.clip(0, 1), 0.03);
    mmdThrs = Lag.kr(mmdThrs.clip(0, 1), 0.03);
    mmdMix = Lag.kr(mmdMix.clip(0, 1), 0.03);
    mmdRat = Lag.kr(mmdRat.clip(0, 1), 0.03);
    mmdGain = Lag.kr(mmdGain.clip(0, 1), 0.03);
    mmdRms = Lag.kr(mmdRms.clip(0, 1), 0.03);
    mmdInp = Lag.kr(mmdInp.clip(0, 1), 0.03);

    // Manual-confirmed endpoints: ATK 0.5..100 ms, REL 50 ms..5 s,
    // RAT 1:1..1:255. Remaining electrical curves are informed estimates.
    atk = mmdAtk.linexp(0, 1, 0.0005, 0.1);
    rel = mmdRel.linexp(0, 1, 0.05, 5);
    thresholdDb = mmdThrs.linlin(0, 1, -63.5, 0);
    ratio = 255 / (255 - (254 * mmdRat));
    makeupDb = mmdGain.linlin(0, 1, 0, 24);
    rmsTime = mmdRms.squared.linlin(0, 1, 0.0001, 0.2);
    // Raw 64 is exactly unity; endpoints are approximately -12/+11.8 dB.
    inputGain = (((mmdInp * 127) - 64) * (12 / 64)).dbamp;
    input = stereoDry * inputGain;

    // Array:maxItem compares in the language and cannot compare UGens.
    peakLevel = input[0].abs.max(input[1].abs);
    rmsLevel = Lag.ar(((input[0].squared + input[1].squared) * 0.5), rmsTime).sqrt;
    detector = (peakLevel * (1 - mmdRms)) + (rmsLevel * mmdRms);
    levelDb = detector.max(1e-9).ampdb;
    overDb = (levelDb - thresholdDb).max(0);
    reductionTargetDb = overDb * (1 - ratio.reciprocal);
    reductionDb = LagUD.ar(reductionTargetDb, atk, rel);
    compressed = input * reductionDb.neg.dbamp * makeupDb.dbamp;
    // A fixed soft clip catches makeup-gain overshoot.
    clipped = compressed.tanh;
    mixed = (input * (1 - mmdMix)) + (clipped * mmdMix);

    if(numChannels > 1) {
        ReplaceOut.ar(out, mixed);
    } {
        ReplaceOut.ar(out, (mixed[0] + mixed[1]) * 0.5);
    };
}, [\ir] ++ Array.fill(8, { \kr })).add;

~dirt.addModule(\mmd, { |dirtEvent|
    dirtEvent.sendSynth(effectName.asString ++ numChannels, [
        out: ~out,
        mmdAtk: (~mmdAtk ? (64 / 127)).clip(0, 1),
        mmdRel: (~mmdRel ? (20 / 127)).clip(0, 1),
        mmdThrs: (~mmdThrs ? (88 / 127)).clip(0, 1),
        mmdMix: (~mmdMix ? 1).clip(0, 1),
        mmdRat: (~mmdRat ? (112 / 127)).clip(0, 1),
        mmdGain: (~mmdGain ? (48 / 127)).clip(0, 1),
        mmdRms: (~mmdRms ? (16 / 127)).clip(0, 1),
        mmdInp: (~mmdInp ? (64 / 127)).clip(0, 1)
    ]);
}, {
    ~mmdAtk.notNil or: { ~mmdRel.notNil } or: { ~mmdThrs.notNil }
    or: { ~mmdMix.notNil } or: { ~mmdRat.notNil }
    or: { ~mmdGain.notNil } or: { ~mmdRms.notNil }
    or: { ~mmdInp.notNil }
});

s.sync;
"mmd per-event dynamics processor loaded".postln;
)
