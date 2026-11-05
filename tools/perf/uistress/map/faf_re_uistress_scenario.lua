version = 3
-- faf_re_uistress: UI stress run map (tools/scenarios/1p-uistress.yaml).
--
-- Terrain comes from any real map: copy its .scmap and _save.lua into this
-- folder as faf_re_uistress.scmap / faf_re_uistress_save.lua (this run used
-- dualgap_adaptive.v0014). The script spawns the tank blocks and a factory;
-- the engine's /map launcher turns every army below ARMY_1 into a rush AI, so
-- the battle is real and the UI has live units to project.
--
-- Eight armies, one team each: the AI fights itself while the local player
-- stays passive, which keeps the sim busy without the stress depending on the
-- player doing anything.
ScenarioInfo = {
    name = "FAF RE UI stress",
    description = "Tank blocks and AI armies for the UI stress runs.",
    type = 'skirmish',
    starts = true,
    preview = '',
    size = {1024, 1024},
    map = '/maps/faf_re_uistress/faf_re_uistress.scmap',
    save = '/maps/faf_re_uistress/faf_re_uistress_save.lua',
    script = '/maps/faf_re_uistress/faf_re_uistress_script.lua',
    norushradius = 0.000000,
    Configurations = {
        ['standard'] = {
            teams = {
                {
                    name = 'FFA',
                    armies = {
                        'ARMY_1', 'ARMY_2', 'ARMY_3', 'ARMY_4',
                        'ARMY_5', 'ARMY_6', 'ARMY_7', 'ARMY_8',
                    },
                },
            },
            customprops = {
                ['ExtraArmies'] = STRING( 'ARMY_9 NEUTRAL_CIVILIAN' ),
            },
        },
    }}
