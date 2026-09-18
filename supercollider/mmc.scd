(
// MMC: 6-10 linearly-interpolated taps with inverted filtered feedback.
var effectName = \dirt_mmc;
var numChannels = ~dirt.numChannels;

SynthDef(effectName.asString ++ numChannels, {
    |out, mmcDel = 0.503937, mmcDep = 0.3, mmcSpd = 0,
    mmcMix = 1, mmcFb = 1, mmcWid = 1, mmcLp = 1, mmcInp = 1, mmcTaps = 0|

    var source = In.ar(out, numChannels);
    var stereo = [source[0], if(numChannels > 1, { source[1] }, { source[0] })];
    var raw = [mmcDel, mmcDep, mmcSpd, mmcMix, mmcFb, mmcWid, mmcLp, mmcInp]
        .clip(0, 1).linlin(0, 1, 0, 127).round;
    var p = raw / 128;
    var dry = stereo * (4 * p[7].squared);
    var feedback = LocalIn.ar(2);
    var fbGain = 0 - (p[4] + (16r00fd71 / 8388608));
    var delayInput = (dry * ((1 - fbGain) * 0.5)) + (feedback * fbGain);
    var centre = (14.91992352 + (992 * p[0])) / 44100;
    var excursion = (502.34559759 * p[1]) / 44100;
    var speed = p[2].squared * (16r000956 / 8388608) * 44100 / (2pi);
    var phase = LFSaw.ar(speed, 1, pi, pi) + 5.92750579;
    var tapTotal = (mmcTaps.clip(0, 1) * 4).round + 6;
    var leftCount = ((tapTotal + 1) * 0.5).floor;
    var rightCount = (tapTotal * 0.5).floor;
    var leftTimes = Array.fill(5, { |i|
        (centre + ((phase + (2pi * i / leftCount)).sin * excursion))
            .clip(SampleDur.ir, 0.19)
    });
    var rightTimes = Array.fill(5, { |i|
        (centre + ((phase + (2pi * i / rightCount) + (pi * p[5])).sin * excursion))
            .clip(SampleDur.ir, 0.19)
    });
    var wet = [
        Mix(Array.fill(5, { |i| DelayL.ar(delayInput[0], 0.2, leftTimes[i])
            * (leftCount > i) })) / leftCount,
        Mix(Array.fill(5, { |i| DelayL.ar(delayInput[1], 0.2, rightTimes[i])
            * (rightCount > i) })) / rightCount
    ];
    var x = raw[6] / 127;
    var logPole = ((((((-11.8579733 * x + 26.22141973) * x - 22.32888259)
        * x + 8.98576210) * x - 1.69233625) * x + 7.66680329) * x - 6.11510228);
    var coefficient = 1 - exp(0 - exp(logPole));
    var cutoff = (0 - log(1 - coefficient)) * SampleRate.ir / (2pi);
    var filtered = OnePole.ar(wet, exp(-2pi * cutoff / SampleRate.ir));
    var mixed = (dry * (1 - p[3])) + (wet * p[3]);

    LocalOut.ar(filtered);
    ReplaceOut.ar(out, if(numChannels > 1, { mixed }, { Mix(mixed) * 0.5 }));
}, [\ir] ++ Array.fill(9, { \kr })).add;

~dirt.addModule(\mmc, { |dirtEvent|
    dirtEvent.sendSynth(effectName.asString ++ numChannels, [
        out: ~out,
        mmcDel: (~mmcDel ? 0.503937).clip(0, 1), mmcDep: (~mmcDep ? 0.3).clip(0, 1),
        mmcSpd: (~mmcSpd ? 0).clip(0, 1), mmcMix: (~mmcMix ? 1).clip(0, 1),
        mmcFb: (~mmcFb ? 1).clip(0, 1), mmcWid: (~mmcWid ? 1).clip(0, 1),
        mmcLp: (~mmcLp ? 1).clip(0, 1), mmcInp: (~mmcInp ? 1).clip(0, 1),
        mmcTaps: (~mmcTaps ? 0).clip(0, 1)
    ]);
}, {
    ~mmcDel.notNil or: { ~mmcDep.notNil } or: { ~mmcSpd.notNil }
    or: { ~mmcMix.notNil } or: { ~mmcFb.notNil } or: { ~mmcWid.notNil }
    or: { ~mmcLp.notNil } or: { ~mmcInp.notNil } or: { ~mmcTaps.notNil }
});

s.sync;
"mmc 6-10 tap chorus loaded".postln;
)
