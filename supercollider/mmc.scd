(
// MMC: six linearly-interpolated taps, inverted filtered feedback and the
// original 0..127 control domain. Native SC is floating point; the VST3 is the
// reference implementation when fixed-point quantisation matters.
var effectName = \dirt_mmc;
var numChannels = ~dirt.numChannels;

SynthDef(effectName.asString ++ numChannels, {
    |out, mmcDel = 0.503937, mmcDep = 0.3, mmcSpd = 0,
    mmcMix = 1, mmcFb = 1, mmcWid = 1, mmcLp = 1, mmcInp = 1|

    var source = In.ar(out, numChannels);
    var stereo = [source[0], if(numChannels > 1, { source[1] }, { source[0] })];
    var raw = [mmcDel, mmcDep, mmcSpd, mmcMix, mmcFb, mmcWid, mmcLp, mmcInp]
        .clip(0, 1).linlin(0, 1, 0, 127).round;
    var p = raw / 128;
    var dry = stereo * (4 * p[7].squared);
    var feedback = LocalIn.ar(2);
    var fbGain = 0 - (p[4] + (16r00fd71 / 8388608));
    var delayInput = (dry * ((1 - fbGain) * 0.5)) + (feedback * fbGain);
    var centre = (16 + (992 * p[0])) / 44100;
    var excursion = (992 * p[1]) / 44100;
    var speed = p[2].squared * (16r000956 / 8388608) * 44100;
    var phase = LFSaw.ar(speed, 1, pi, pi);
    var tapPhase = phase + [0, 2pi / 3, 4pi / 3];
    var leftTimes = (centre + (tapPhase.sin * excursion)).clip(SampleDur.ir, 0.19);
    var rightTimes = (centre + ((tapPhase + (pi * p[5])).sin * excursion))
        .clip(SampleDur.ir, 0.19);
    var wet = [
        Mix(DelayL.ar(delayInput[0], 0.2, leftTimes)) * (16r2aaaab / 8388608),
        Mix(DelayL.ar(delayInput[1], 0.2, rightTimes)) * (16r2aaaab / 8388608)
    ];
    var x = raw[6] / 127;
    // Degree-six fit of the recovered 128-entry coefficient law (max error
    // < 0.00052); convert its one-pole coefficient to an equivalent cutoff.
    var logPole = ((((((-11.8579733 * x + 26.22141973) * x - 22.32888259)
        * x + 8.98576210) * x - 1.69233625) * x + 7.66680329) * x - 6.11510228);
    var coefficient = 1 - exp(0 - exp(logPole));
    var cutoff = (0 - log(1 - coefficient)) * SampleRate.ir / (2pi);
    var filtered = OnePole.ar(wet, exp(-2pi * cutoff / SampleRate.ir));
    var mixed = (dry * (1 - p[3])) + (wet * p[3]);

    LocalOut.ar(filtered);
    ReplaceOut.ar(out, if(numChannels > 1, { mixed }, { Mix(mixed) * 0.5 }));
}, [\ir] ++ Array.fill(8, { \kr })).add;

~dirt.addModule(\mmc, { |dirtEvent|
    dirtEvent.sendSynth(effectName.asString ++ numChannels, [
        out: ~out,
        mmcDel: (~mmcDel ? 0.503937).clip(0, 1), mmcDep: (~mmcDep ? 0.3).clip(0, 1),
        mmcSpd: (~mmcSpd ? 0).clip(0, 1), mmcMix: (~mmcMix ? 1).clip(0, 1),
        mmcFb: (~mmcFb ? 1).clip(0, 1), mmcWid: (~mmcWid ? 1).clip(0, 1),
        mmcLp: (~mmcLp ? 1).clip(0, 1), mmcInp: (~mmcInp ? 1).clip(0, 1)
    ]);
}, {
    ~mmcDel.notNil or: { ~mmcDep.notNil } or: { ~mmcSpd.notNil }
    or: { ~mmcMix.notNil } or: { ~mmcFb.notNil } or: { ~mmcWid.notNil }
    or: { ~mmcLp.notNil } or: { ~mmcInp.notNil }
});

s.sync;
"mmc six-tap chorus loaded".postln;
)
