# Game Design Document

## Concept

Defn is a 2.5D belt-scroller tug-of-war defense game with light strategy and real-time pressure. The player protects a fortified base on the left side of a wide battlefield while hostile waves advance from the right.

## Core Loop

Each mission asks the player to spend regenerating energy and enemy bounties on timely unit deployments, while combat resolves automatically once opposing units enter range. Hovering a selectable friendly previews its ground marker before selection. The player can select one mobile friendly and order it to fall back horizontally, trading battlefield ground and attack time for survival or a better engagement. Different friendly and hostile units fulfill different battlefield roles, creating decisions around timing, economy, pressure, composition, and lightweight repositioning.

### The battle line

The battle line is the core mechanic. Hostiles advance from the right, friendlies hold from the left, and everything is decided where the two meet: a unit fights the first enemy it reaches and holds there until one side gives, so ground changes hands only by winning the fight at the line. Units do not pass through the opposing line in either direction, and nothing on either side treats the base as a destination to run for. The diver, which sprints past the front rank to fight in contact, is the one exception; it is tolerated rather than a template. The base is a property of a level, not of the game: a mode may have no base or several, so no rule, unit or difficulty lever may assume one. Difficulty is applied at the line, by what arrives there and how hard it is to hold, never by routing around it.

## Progression

A mission is won by defeating all scheduled waves before the base loses all integrity, with score awarded for kills, remaining integrity, and completion. The campaign advances through chained level clears, increasing enemy variety, wave density, and pressure over time. Between runs, upgrade drafts convert performance into strategic permanent improvements, expanding the roster and strengthening future attempts.
