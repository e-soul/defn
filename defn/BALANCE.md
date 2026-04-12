# Balance Strategy

## Unit Budget

Comparison formula inside each side of the roster:

$$
	ext{power} = \sqrt{\text{hp} \times \text{ranged dps}} \times \text{reach} \times \text{speed} \times \text{aoe}
$$

The multipliers should stay close to 1.0. Range and speed matter a lot, so small changes are enough.

- Breacher: cheapest anchor, highest hp, shortest reach.
- Marksman: longest range, best pick into jackal and mason.
- Impact: fastest unit, aggressive mid-range pressure.
- Operator: safest sustained general-purpose fire.
- Grime: baseline pressure.
- Mason: splash tax on clustered friendlies.
- Wrecker: main direct combat threat.
- Jackal: long-range pressure check.

## Level Tuning

Tune each mission with two numbers:

- Total threat.
- Peak enemy count inside a 5-second window.

Threat points:

- Grime: 1.0
- Mason: 1.4
- Wrecker: 1.8
- Jackal: 1.6

Current targets:

- Level 1: about 16 threat, peak 6.
- Level 2: about 30 threat, peak 6.
- Level 3: about 41 threat, peak 7.
- Level 4: about 58 threat, peak 8.

If a level is too easy, lower starting energy or tighten spawn spacing first. If a level is too hard, reduce spike density before reducing total threat.


## Checklist

1. Compare units with the power formula.
1. Check that every unit still has a distinct job.
1. Recompute total threat and peak 5-second spike.
