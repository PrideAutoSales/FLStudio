"""Cruz Drums: the sound of each genre, as ranges the voices pick from.

The targets follow what the best-selling one-shot packs in each style have in common:
house kicks short and tuned with a click, trap 808s long with drive and hats tight and
bright, boom bap dusty and sampler-crunched, pop clean and wide, disco and funk live and
taped, drum and bass tight kicks with high cracking snares.
"""

HOUSE = dict(
    kick_notes=(31, 37), kick_decay=(0.26, 0.55), kick_sweep=(3.5, 6.5), kick_tau=(0.010, 0.028),
    kick_click=(0.15, 0.7), kick_drive=(1.4, 3.6),
    snare_tone=(190, 260), snare_noise_decay=(0.14, 0.3), snare_hp=(1500, 3000), snare_drive=(1.2, 2.2),
    snare_room=(0.08, 0.3), snare_clap=0.3,
    clap_center=(1000, 1700), clap_tail=(0.12, 0.3), clap_layers=(1, 3), clap_room=(0.12, 0.35),
    rim_kinds=["808", "click", "808", "wood"],
    hat_metal=(0.5, 0.85), hat_scale=(1.0, 1.6), hat_closed=(0.04, 0.1), hat_open=(0.3, 0.6), open_frac=0.4,
    percs=["conga", "bongo", "cowbell", "shaker", "woodblock", "tick", "tambourine", "tom", "clave", "agogo"],
)

HIPHOP = dict(
    kick_notes=(29, 35), kick_decay=(0.35, 0.7), kick_sweep=(3.0, 5.5), kick_tau=(0.012, 0.03),
    kick_click=(0.05, 0.45), kick_drive=(2.0, 4.2),
    kick_808=0.25, kick_808_notes=(26, 34), kick_808_decay=(1.0, 1.8), kick_808_drive=(2.0, 4.0),
    snare_tone=(165, 230), snare_noise_decay=(0.18, 0.36), snare_hp=(1200, 2500), snare_drive=(1.5, 3.0),
    snare_room=(0.15, 0.4), snare_clap=0.4,
    clap_center=(950, 1500), clap_tail=(0.15, 0.32), clap_layers=(2, 4), clap_room=(0.15, 0.4),
    hat_metal=(0.4, 0.75), hat_closed=(0.03, 0.08), hat_open=(0.25, 0.5), open_frac=0.3,
    percs=["snap", "conga", "tom", "shaker", "woodblock", "metal", "pluck", "tambourine", "bongo", "timbale"],
)

TRAP = dict(
    kick_notes=(30, 36), kick_decay=(0.22, 0.45), kick_sweep=(4.5, 8.0), kick_tau=(0.008, 0.02),
    kick_click=(0.4, 0.9), kick_drive=(2.5, 5.0),
    kick_808=0.5, kick_808_notes=(24, 34), kick_808_decay=(1.3, 2.6), kick_808_drive=(2.0, 6.0),
    snare_tone=(200, 285), snare_noise_decay=(0.14, 0.3), snare_hp=(2000, 4000), snare_drive=(1.8, 3.2),
    snare_room=(0.1, 0.32), snare_clap=0.5,
    clap_center=(1200, 2000), clap_tail=(0.14, 0.32), clap_layers=(2, 5), clap_room=(0.2, 0.5),
    clap_drive=(1.3, 2.6),
    rim_kinds=["808", "click", "808", "wood"],
    hat_metal=(0.6, 0.9), hat_scale=(1.2, 1.8), hat_hp=(7000, 9000), hat_closed=(0.02, 0.05),
    hat_open=(0.2, 0.45), open_frac=0.3, hat_drive=(1.2, 2.2),
    percs=["pluck", "metal", "snap", "zap", "woodblock", "tom", "shaker", "clave", "triangle", "cowbell"],
)

BOOMBAP = dict(
    kick_notes=(31, 39), kick_decay=(0.24, 0.45), kick_sweep=(2.5, 4.5), kick_tau=(0.018, 0.04),
    kick_click=(0.1, 0.5), kick_drive=(2.0, 4.0), kick_knock=(0.2, 0.7), kick_lp=(7000, 10000),
    snare_tone=(165, 230), snare_noise_decay=(0.15, 0.3), snare_hp=(1000, 2200), snare_drive=(2.0, 3.5),
    snare_room=(0.2, 0.45), room_decay=(0.3, 0.6), room_lp=(4000, 6500), snare_ring=0.4,
    clap_center=(900, 1400), clap_tail=(0.12, 0.28), clap_layers=(1, 4), clap_room=(0.15, 0.4),
    clap_drive=(1.5, 2.8),
    rim_kinds=["wood", "click", "wood", "808"],
    hat_metal=(0.3, 0.6), hat_closed=(0.03, 0.09), hat_open=(0.25, 0.5), open_frac=0.3, hat_acoustic=0.5,
    hat_drive=(1.3, 2.4),
    percs=["tambourine", "shaker", "conga", "bongo", "woodblock", "tom", "snap", "cowbell", "timbale", "clave"],
    crush=True, crush_lp=(7000, 10500), vinyl=True,
)

POP = dict(
    kick_notes=(30, 36), kick_decay=(0.3, 0.55), kick_sweep=(3.5, 6.0), kick_tau=(0.01, 0.025),
    kick_click=(0.3, 0.65), kick_drive=(1.5, 3.0),
    kick_808=0.15, kick_808_notes=(26, 34), kick_808_decay=(1.0, 1.6), kick_808_drive=(1.5, 3.0),
    snare_tone=(180, 245), snare_noise_decay=(0.2, 0.4), snare_hp=(1500, 3000), snare_drive=(1.3, 2.2),
    snare_room=(0.25, 0.5), room_lp=(8000, 11000), snare_clap=0.6,
    clap_center=(1100, 1800), clap_tail=(0.16, 0.34), clap_layers=(2, 5), clap_room=(0.25, 0.5),
    hat_metal=(0.45, 0.75), hat_closed=(0.03, 0.08), hat_open=(0.3, 0.6), open_frac=0.3,
    percs=["snap", "shaker", "tambourine", "clave", "tom", "woodblock", "triangle", "pluck", "conga", "metal"],
    perc_room=(0.1, 0.35),
)

DISCO = dict(
    kick_notes=(33, 39), kick_decay=(0.2, 0.36), kick_sweep=(2.0, 3.5), kick_tau=(0.015, 0.03),
    kick_click=(0.2, 0.5), kick_drive=(1.2, 2.2), kick_knock=(0.1, 0.4),
    snare_tone=(175, 225), snare_noise_decay=(0.2, 0.36), snare_hp=(800, 1500), snare_drive=(1.2, 2.0),
    snare_room=(0.15, 0.32), snare_ring=0.6,
    clap_center=(900, 1400), clap_tail=(0.14, 0.3), clap_layers=(3, 5), clap_room=(0.22, 0.4),
    rim_kinds=["wood", "wood", "click"],
    hat_closed=(0.04, 0.1), hat_open=(0.35, 0.7), open_frac=0.4, hat_acoustic=0.8, hat_room=(0.05, 0.2),
    percs=["syndrum", "conga", "cowbell", "tambourine", "guiro", "clave", "bongo", "timbale", "triangle", "agogo"],
    tape=True,
)

FUNK = dict(
    kick_notes=(33, 39), kick_decay=(0.18, 0.3), kick_sweep=(2.0, 3.5), kick_tau=(0.012, 0.025),
    kick_click=(0.3, 0.6), kick_drive=(1.3, 2.5), kick_knock=(0.1, 0.4),
    snare_tone=(195, 265), snare_noise_decay=(0.12, 0.22), snare_hp=(1200, 2500), snare_drive=(1.4, 2.4),
    snare_room=(0.04, 0.16), snare_ring=0.5,
    clap_center=(1000, 1500), clap_tail=(0.1, 0.2), clap_layers=(1, 3), clap_room=(0.04, 0.18),
    rim_kinds=["wood", "click", "wood"], rim_room=(0.0, 0.15),
    hat_closed=(0.03, 0.07), hat_open=(0.25, 0.45), open_frac=0.3, hat_acoustic=0.75,
    percs=["cowbell", "conga", "guiro", "tambourine", "timbale", "bongo", "woodblock", "agogo", "clave", "shaker"],
    perc_room=(0.0, 0.15), tape=True,
)

DNB = dict(
    kick_notes=(31, 37), kick_decay=(0.18, 0.32), kick_sweep=(5.0, 9.0), kick_tau=(0.006, 0.015),
    kick_click=(0.5, 0.95), kick_drive=(2.0, 4.0),
    snare_tone=(220, 300), snare_noise_decay=(0.18, 0.35), snare_hp=(2500, 4500), snare_drive=(2.0, 3.4),
    snare_room=(0.18, 0.4), snare_clap=0.3,
    clap_center=(1300, 2000), clap_tail=(0.14, 0.28), clap_layers=(2, 4), clap_room=(0.2, 0.4),
    hat_metal=(0.5, 0.8), hat_scale=(1.2, 1.7), hat_closed=(0.02, 0.06), hat_open=(0.2, 0.4), open_frac=0.3,
    percs=["shaker", "zap", "metal", "tom", "tick", "woodblock", "tambourine", "conga", "snap", "pluck"],
)

GENRES = {
    "House": HOUSE, "Hip Hop": HIPHOP, "Trap": TRAP, "Boom Bap": BOOMBAP,
    "Pop": POP, "Disco": DISCO, "Funk": FUNK, "Drum and Bass": DNB,
}
