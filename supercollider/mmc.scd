(
// 2 x 3 tap stereo chorus.
// Public controls are normalized to 0..1 for Tidal. Missing controls use the
// Performance defaults: MIX 1, DEP 0.3, SPD 0, FB 1, INP 1, WID 1.
// DEL remains at raw 64/127 and LP remains fully open.
var effectName = \dirt_mmc;
var numChannels = ~dirt.numChannels;

SynthDef(effectName.asString ++ numChannels, {
    |out,
    mmcDel = 0.503937, mmcDep = 0.3, mmcSpd = 0,
    mmcMix = 1, mmcFb = 1, mmcWid = 1, mmcLp = 1,
    mmcInp = 1|

    var dry = In.ar(out, numChannels);
    var left = dry[0];
    var right = if(numChannels > 1, { dry[1] }, { dry[0] });
    var stereoDry = [left, right];
    var delaySeconds, depthSeconds, speedHz;
    var mixAmount, feedbackGain, widthAmount, cutoffHz, inputGain;
    var input, feedback, feedbackFiltered, delayInput;
    var rateRatios, phases, leftMod, rightMod;
    var leftTimes, rightTimes, wetRaw, wetMid, wetSide, wet, mixed;

    mmcDel = Lag.kr(mmcDel.clip(0, 1), 0.04);
    mmcDep = Lag.kr(mmcDep.clip(0, 1), 0.04);
    mmcSpd = Lag.kr(mmcSpd.clip(0, 1), 0.08);
    mmcMix = Lag.kr(mmcMix.clip(0, 1), 0.04);
    mmcFb = Lag.kr(mmcFb.clip(0, 1), 0.06);
    mmcWid = Lag.kr(mmcWid.clip(0, 1), 0.06);
    mmcLp = Lag.kr(mmcLp.clip(0, 1), 0.06);
    mmcInp = Lag.kr(mmcInp.clip(0, 1), 0.04);

    // Compact musical curves for delay, modulation, and feedback controls.
    delaySeconds = mmcDel.linexp(0, 1, 0.0006, 0.025);
    depthSeconds = mmcDep.linlin(0, 1, 0, 0.012);
    speedHz = mmcSpd.linexp(0, 1, 0.025, 8);
    mixAmount = mmcMix;
    feedbackGain = mmcFb.linlin(0, 1, 0, 0.99);
    widthAmount = mmcWid;
    cutoffHz = mmcLp.linexp(0, 1, 180, 20000)
        .clip(20, SampleRate.ir * 0.45);
    inputGain = 2 ** (((mmcInp * 127) - 64) / 32);
    input = stereoDry * inputGain;

    // LP is confined to the feedback path, matching the documented machine.
    feedback = LocalIn.ar(2);
    feedbackFiltered = LPF.ar(LeakDC.ar(feedback), cutoffHz);
    delayInput = input + (feedbackFiltered * feedbackGain);

    rateRatios = [0.93, 1, 1.071];
    phases = [0, 2pi / 3, 4pi / 3];
    leftMod = SinOsc.kr(speedHz * rateRatios, phases);
    rightMod = SinOsc.kr(
        speedHz * rateRatios.reverse,
        phases + (pi * (0.35 + (0.65 * widthAmount)))
    );
    leftTimes = (delaySeconds + (depthSeconds * leftMod))
        .clip(0.0001, 0.04);
    rightTimes = (delaySeconds + (depthSeconds * rightMod))
        .clip(0.0001, 0.04);

    wetRaw = [
        Mix(DelayC.ar(delayInput[0], 0.04, leftTimes)) / 3,
        Mix(DelayC.ar(delayInput[1], 0.04, rightTimes)) / 3
    ];
    wetMid = (wetRaw[0] + wetRaw[1]) * 0.5;
    wetSide = (wetRaw[0] - wetRaw[1]) * 0.5 * widthAmount;
    wet = [wetMid + wetSide, wetMid - wetSide];

    LocalOut.ar(tanh(wetRaw));
    mixed = XFade2.ar(input, wet, (mixAmount * 2) - 1);

    if(numChannels > 1) {
        ReplaceOut.ar(out, mixed);
    } {
        ReplaceOut.ar(out, (mixed[0] + mixed[1]) * 0.5);
    };
}, [\ir] ++ Array.fill(8, { \kr })).add;

~dirt.addModule(\mmc, { |dirtEvent|
    dirtEvent.sendSynth(effectName.asString ++ numChannels, [
        out: ~out,
        mmcDel: (~mmcDel ? 0.503937).clip(0, 1),
        mmcDep: (~mmcDep ? 0.3).clip(0, 1),
        mmcSpd: (~mmcSpd ? 0).clip(0, 1),
        mmcMix: (~mmcMix ? 1).clip(0, 1),
        mmcFb: (~mmcFb ? 1).clip(0, 1),
        mmcWid: (~mmcWid ? 1).clip(0, 1),
        mmcLp: (~mmcLp ? 1).clip(0, 1),
        mmcInp: (~mmcInp ? 1).clip(0, 1)
    ]);
}, {
    ~mmcDel.notNil or: { ~mmcDep.notNil } or: { ~mmcSpd.notNil }
    or: { ~mmcMix.notNil } or: { ~mmcFb.notNil }
    or: { ~mmcWid.notNil } or: { ~mmcLp.notNil }
    or: { ~mmcInp.notNil }
});

s.sync;
"mmc per-event 2x3-tap chorus loaded".postln;
)
