"""Cruz Drums: each genre as recipe weights and processing ranges.

Version 2 is built around the hard-knocking, textured hip hop kit sound: layered
organic sources (skins, wires, hands, tables, boxes, mouths, found objects) pushed
through console-style clipping, distort-EQ-distort, compression and small rooms,
then mastered loud without digital clipping. Each genre bends that sound its own way.
"""

HIPHOP = dict(
    kick={"Knock": 4, "Slam": 2, "Box": 1, "Mouth": 1, "808": 2, "Thump": 1},
    kick_notes=(29, 36), kick_decay=(0.3, 0.6), k808_decay=(1.0, 1.8),
    snare={"Layered": 3, "Clapper": 2, "Slam": 2, "Acoustic": 1, "Box": 1, "Mouth": 1, "Crack": 1},
    snare_tune=(165, 235), snare_room=(0.1, 0.35),
    clap={"Group": 2, "Snap Stack": 2, "Slap": 1, "Tight": 1, "Mouth": 1},
    rim={"Rimshot": 2, "Cross Stick": 2, "Knuckle": 1, "Glass": 1, "Can": 1, "Stick Click": 1, "Tongue": 1},
    hat={"Closed": 4, "Open": 2, "Pedal": 1, "Shaker": 1, "Mouth": 1, "Keys": 1, "Sizzle": 1},
    percs=["Snap", "Knuckle", "Can", "Shaker", "Tongue Click", "Bottle", "Conga", "Paper", "Box Tap", "Tambourine", "Lip Pop"],
    drive1=(2.0, 4.5), drive2=(1.5, 3.0), presence=(2, 6), clip=(1.3, 1.9), grit=(0.004, 0.012),
    varispeed=(0.85, 0.97), varispeed_p=0.5,
)

TRAP = dict(
    kick={"808": 5, "Knock": 3, "Punch": 2, "Slam": 1},
    kick_notes=(25, 35), kick_decay=(0.25, 0.45), k808_decay=(1.3, 2.4),
    snare={"Crack": 3, "Clapper": 3, "Layered": 2, "Slam": 1, "Mouth": 1},
    snare_tune=(200, 270), snare_room=(0.1, 0.3),
    clap={"Snap Stack": 3, "Group": 2, "Tight": 1, "Slap": 1},
    rim={"Rimshot": 2, "Stick Click": 2, "Glass": 1, "Can": 1, "Tongue": 1, "Cross Stick": 1},
    hat={"Closed": 6, "Open": 2, "Keys": 1, "Mouth": 1, "Sizzle": 1},
    hat_closed=(0.02, 0.05), hat_open=(0.25, 0.5),
    percs=["Snap", "Can", "Bottle", "Tongue Click", "Keys", "Woodblock", "Tom", "Paper", "Knuckle", "Lip Pop", "Triangle"],
    drive1=(2.0, 5.0), drive2=(1.5, 3.5), presence=(3, 7), clip=(1.3, 2.0), grit=(0.002, 0.006),
)

BOOMBAP = dict(
    kick={"Thump": 3, "Knock": 3, "Slam": 2, "Box": 2},
    kick_notes=(31, 39), kick_decay=(0.25, 0.45), kick_room=(0.0, 0.2),
    snare={"Acoustic": 3, "Rimshot": 2, "Layered": 2, "Slam": 2, "Box": 1},
    snare_tune=(165, 230), snare_room=(0.2, 0.45),
    clap={"Group": 3, "Slap": 2, "Tight": 1, "Snap Stack": 1},
    rim={"Cross Stick": 3, "Rimshot": 2, "Knuckle": 2, "Stick Click": 1, "Can": 1},
    hat={"Closed": 4, "Open": 2, "Pedal": 2, "Shaker": 1, "Sizzle": 1},
    percs=["Tambourine", "Shaker", "Conga", "Bongo", "Knuckle", "Box Tap", "Snap", "Woodblock", "Can", "Paper", "Cowbell"],
    drive1=(2.0, 4.0), drive2=(1.5, 3.0), presence=(1, 4), clip=(1.2, 1.7), grit=(0.008, 0.02),
    room_damp=(3500, 6000), crush=True, crush_lp=(7000, 10500), varispeed=(0.78, 0.94), varispeed_p=0.7,
)

HOUSE = dict(
    kick={"Punch": 4, "Knock": 3, "Thump": 2, "Box": 1},
    kick_notes=(31, 37), kick_decay=(0.25, 0.5),
    snare={"Clapper": 3, "Layered": 2, "Acoustic": 2, "Rimshot": 1, "Box": 1},
    snare_tune=(190, 260), snare_room=(0.15, 0.35),
    clap={"Group": 4, "Tight": 2, "Snap Stack": 1, "Slap": 1},
    rim={"Rimshot": 2, "Stick Click": 2, "Cross Stick": 2, "Glass": 1, "Can": 1},
    hat={"Closed": 4, "Open": 3, "Shaker": 2, "Pedal": 1, "Sizzle": 1},
    percs=["Conga", "Bongo", "Shaker", "Cowbell", "Woodblock", "Tambourine", "Clave", "Can", "Bottle", "Tom", "Snap"],
    drive1=(1.5, 3.0), drive2=(1.2, 2.2), presence=(2, 5), clip=(1.2, 1.6), grit=(0.001, 0.004),
)

POP = dict(
    kick={"Punch": 3, "Knock": 3, "808": 1, "Thump": 1},
    kick_notes=(30, 36), kick_decay=(0.3, 0.5), k808_decay=(1.0, 1.6),
    snare={"Clapper": 3, "Layered": 3, "Acoustic": 1, "Crack": 1, "Mouth": 1},
    snare_tune=(180, 245), snare_room=(0.25, 0.5),
    clap={"Group": 3, "Snap Stack": 3, "Tight": 1},
    rim={"Stick Click": 2, "Rimshot": 2, "Cross Stick": 1, "Glass": 1, "Tongue": 1},
    hat={"Closed": 4, "Open": 2, "Shaker": 2, "Mouth": 1, "Keys": 1},
    percs=["Snap", "Shaker", "Tambourine", "Clave", "Tom", "Woodblock", "Triangle", "Tongue Click", "Bottle", "Paper", "Conga"],
    drive1=(1.4, 2.8), drive2=(1.2, 2.0), presence=(3, 6), clip=(1.2, 1.6), grit=(0.0, 0.002),
    room_damp=(7000, 11000),
)

DISCO = dict(
    kick={"Thump": 4, "Knock": 2, "Box": 1},
    kick_notes=(33, 39), kick_decay=(0.2, 0.36), kick_room=(0.0, 0.15),
    snare={"Acoustic": 4, "Rimshot": 2, "Layered": 2},
    snare_tune=(175, 225), snare_room=(0.15, 0.32),
    clap={"Group": 5, "Slap": 1, "Tight": 1},
    rim={"Cross Stick": 3, "Rimshot": 2, "Stick Click": 1},
    hat={"Closed": 4, "Open": 3, "Pedal": 1, "Shaker": 1, "Sizzle": 1},
    hat_closed=(0.04, 0.1), hat_open=(0.35, 0.8), hat_room=(0.05, 0.2),
    percs=["Conga", "Bongo", "Cowbell", "Tambourine", "Guiro", "Clave", "Tom", "Triangle", "Woodblock", "Shaker", "Bottle"],
    drive1=(1.3, 2.4), drive2=(1.1, 1.8), presence=(1, 4), clip=(1.1, 1.4), grit=(0.002, 0.006), tape=True,
)

FUNK = dict(
    kick={"Thump": 3, "Knock": 3, "Box": 1},
    kick_notes=(33, 39), kick_decay=(0.18, 0.3),
    snare={"Acoustic": 3, "Rimshot": 3, "Crack": 1, "Layered": 1},
    snare_tune=(195, 265), snare_room=(0.04, 0.16),
    clap={"Tight": 2, "Group": 2, "Slap": 2},
    rim={"Cross Stick": 3, "Rimshot": 2, "Stick Click": 1, "Knuckle": 1}, rim_room=(0.0, 0.12),
    hat={"Closed": 5, "Open": 2, "Pedal": 2},
    hat_closed=(0.03, 0.07), hat_open=(0.25, 0.45),
    percs=["Cowbell", "Conga", "Guiro", "Tambourine", "Bongo", "Woodblock", "Clave", "Shaker", "Can", "Tom", "Knuckle"],
    perc_room=(0.0, 0.15),
    drive1=(1.6, 3.0), drive2=(1.2, 2.2), presence=(2, 5), clip=(1.2, 1.6), grit=(0.002, 0.008), tape=True,
)

DNB = dict(
    kick={"Punch": 4, "Knock": 3, "Slam": 1},
    kick_notes=(31, 37), kick_decay=(0.18, 0.32),
    snare={"Crack": 4, "Rimshot": 2, "Layered": 2, "Slam": 1},
    snare_tune=(220, 300), snare_room=(0.18, 0.4),
    clap={"Snap Stack": 2, "Group": 2, "Tight": 1, "Slap": 1},
    rim={"Rimshot": 2, "Stick Click": 2, "Can": 1, "Glass": 1, "Cross Stick": 1},
    hat={"Closed": 5, "Open": 2, "Shaker": 2, "Keys": 1},
    hat_closed=(0.02, 0.06), hat_open=(0.2, 0.4),
    percs=["Shaker", "Can", "Tom", "Woodblock", "Bottle", "Tambourine", "Conga", "Snap", "Keys", "Paper", "Clave"],
    drive1=(2.5, 5.0), drive2=(1.8, 3.5), presence=(4, 8), clip=(1.4, 2.0), grit=(0.002, 0.006),
)

GENRES = {
    "House": HOUSE, "Hip Hop": HIPHOP, "Trap": TRAP, "Boom Bap": BOOMBAP,
    "Pop": POP, "Disco": DISCO, "Funk": FUNK, "Drum and Bass": DNB,
}
