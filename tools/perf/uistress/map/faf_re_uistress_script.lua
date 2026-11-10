-- faf_re_uistress: sim half of the UI stress run (tools/scenarios/1p-uistress.yaml).
--
-- The stress comes from unit count, not from issuing orders: the UI rebuilds
-- selection, strategic icons and panel state for everything the camera can
-- see, so a few thousand idle units in one place already load the UI VM.
--
-- Spawned on open land near the start position:
--   - a dense block of idle tanks (STRESS_TANKS), the bulk of the load;
--   - a factory that keeps building tanks so the count keeps drifting up;
--   - a second dense block parked further out, so zooming out and panning
--     re-projects a large, changing set.
-- The camera zoom loop lives in the UI hook (tools/perf/uistress/hook), which
-- is user side only.

local ScenarioUtils = import('/lua/sim/ScenarioUtilities.lua')

local Factory = 'ueb0101'
local Tank = 'uel0201'

-- `Tanks` is the count *per army*, and it scales the load so sim cost can be
-- measured against unit count. Each army gets its own block of that size, so
-- the total is `Armies x Tanks`.
--
-- Keep the per-army count under the unit cap. A /map skirmish starts from
-- `SinglePlayerLaunch.lua`'s `defaultOptions`, which hardcodes `UnitCap = '500'`
-- and exposes no command line override, so any unit past the 500th that
-- `CreateUnitHPR` asks for comes back null and raises `CreateUnitHPR(...)
-- failed`. The cap is per army, so the per-army figure is the one that matters.
--
-- The default of 400 per army stays under that cap for every army the
-- scenario declares, and keeps earlier single-army measurements comparable:
-- with `Armies` at its default of 0 the script spawns into every army the
-- session created, which changes the total but not the per-army figure.
--
-- `BlockColumns` stays 30 so a larger count grows into more rows at the same
-- spacing instead of a denser block the pathfinder has to fight through.
local STRESS_TANKS = tonumber(ScenarioInfo.Options.Tanks) or 400
local BLOCK_COLUMNS = tonumber(ScenarioInfo.Options.BlockColumns) or 30
local BLOCK_SPACING = 2.5

local function IsLand(x, z)
    if x < 64 or z < 64 or x > 960 or z > 960 then
        return false
    end
    return GetTerrainHeight(x, z) >= GetSurfaceHeight(x, z)
end

local function IsOpenLand(x, z, radius)
    if not IsLand(x, z) then
        return false
    end
    for i = 0, 7 do
        local a = i * math.pi / 4
        if not IsLand(x + math.cos(a) * radius, z + math.sin(a) * radius) then
            return false
        end
    end
    return true
end

local function Distance(ax, az, bx, bz)
    return math.sqrt((ax - bx) * (ax - bx) + (az - bz) * (az - bz))
end

local function FindSpot(cx, cz, sx, sz, minFromStart, ox, oz, minFromOther)
    for r = 40, 400, 8 do
        for i = 0, 31 do
            local a = i * math.pi / 16
            local x, z = cx + math.cos(a) * r, cz + math.sin(a) * r
            if IsOpenLand(x, z, 40) and Distance(x, z, sx, sz) >= minFromStart and
                (ox == nil or Distance(x, z, ox, oz) >= minFromOther) then
                return x, z
            end
        end
    end
    return nil
end

local function SpawnBlock(army, centreX, centreZ, count)
    local spawned = 0
    for i = 0, count - 1 do
        local column = math.mod(i, BLOCK_COLUMNS)
        local row = math.floor(i / BLOCK_COLUMNS)
        local x = centreX + (column - BLOCK_COLUMNS * 0.5) * BLOCK_SPACING
        local z = centreZ + (row - BLOCK_COLUMNS * 0.5) * BLOCK_SPACING
        if IsLand(x, z) then
            CreateUnitHPR(Tank, army, x, GetTerrainHeight(x, z), z, 0, 0, 0)
            spawned = spawned + 1
        end
    end
    return spawned
end

function OnPopulate(scenario)
    ScenarioUtils.InitializeArmies()
end

-- `Armies` (default 1) spreads the tanks over that many armies, placed at their
-- own start markers. `Attack` (default 0) makes each army move on the next
-- one's position the moment it spawns: pathing, target acquisition, weapon
-- fire and unit deaths all start, which is the sim work a large match spends
-- its time in. One army with Attack 0 keeps the original idle-block behaviour,
-- which only loads the UI.
--
-- No AI brain is involved: the engine does not expose one to a /map-started
-- skirmish, and none is needed. A move order is enough to exercise the mobile
-- paths; units fire at whatever comes into range on their own.
--
-- `Armies` defaults to 0, meaning "every army this session created": a /map
-- skirmish hands ARMY_2..N to rush AI (`SinglePlayerLaunch.lua`), and those
-- armies want units to command. `/gameoptions` is not passed on that path, so
-- the defaults here are what a /map run actually uses.
--
-- `Attack` defaults to 1: with the blocks parked on their own start markers the
-- armies are a map apart, and idle tanks cost the sim almost nothing. Sending
-- each one at another army makes them path, acquire targets, fire and die, which
-- is the work a real match spends its time in.
local STRESS_ARMIES = tonumber(ScenarioInfo.Options.Armies) or 0
local STRESS_ATTACK = tonumber(ScenarioInfo.Options.Attack) or 1

function OnStart(self)
    ForkThread(function()
        LOG('UISTRESS OnStart enter')
        local start = ScenarioUtils.GetMarker('ARMY_1').position
        local sx, sz = start[1], start[3]
        LOG(string.format('UISTRESS start marker %.0f,%.0f', sx, sz))

        local brainCount = 0
        for _ in pairs(ArmyBrains) do
            brainCount = brainCount + 1
        end
        LOG(string.format('UISTRESS ArmyBrains count=%d', brainCount))

        -- Collect the armies worth giving units to: every one the scenario did
        -- not mark as civilian, and that has a start marker. `ScenarioInfo.ArmySetup`
        -- is the engine's own per-army table (`CArmyImpl` fills it from the launch
        -- info), so its `Civilian` flag is the authoritative test; `BrainType`
        -- names the brain implementation and is not the same thing.
        -- `STRESS_ARMIES` caps the list when a scenario wants fewer.
        local targets = {}
        for index = 1, brainCount do
            local brain = ArmyBrains[index]
            if brain ~= nil and brain.Name ~= nil then
                local marker = ScenarioUtils.GetMarker(string.format('ARMY_%d', index))
                local setup = ScenarioInfo.ArmySetup and ScenarioInfo.ArmySetup[brain.Name]
                local isCivilian = setup ~= nil and setup.Civilian
                if marker ~= nil and not isCivilian then
                    targets[table.getn(targets) + 1] = { index = index, name = brain.Name }
                end
            end
        end
        if STRESS_ARMIES > 0 and table.getn(targets) > STRESS_ARMIES then
            local trimmed = {}
            for i = 1, STRESS_ARMIES do
                trimmed[i] = targets[i]
            end
            targets = trimmed
        end

        local armyCount = table.getn(targets)
        LOG(string.format('UISTRESS armyCount=%d attack=%d tanks=%d', armyCount, STRESS_ATTACK, STRESS_TANKS))
        if armyCount == 0 then
            LOG('UISTRESS no army to place units in')
            return
        end

        -- Plenty of resources, so a factory never stalls and no army has to
        -- manage its economy during the measurement.
        for i = 1, armyCount do
            local b = ArmyBrains[targets[i].index]
            b:GiveStorage('Mass', 1000000)
            b:GiveStorage('Energy', 1000000)
            b:GiveResource('Mass', 1000000)
            b:GiveResource('Energy', 1000000)
        end
        LOG('UISTRESS resources given')

        -- `STRESS_TANKS` is the count *per army*: each one gets its own block of
        -- that size, so the total is `armyCount * STRESS_TANKS`. The unit cap is
        -- per army (500 in a /map skirmish), so as long as this stays under it no
        -- `CreateUnitHPR` comes back null.
        local spawnedTotal = 0
        local armiesSpawned = 0
        local blocks = {}

        for i = 1, armyCount do
            local index = targets[i].index
            local army = targets[i].name
            local markerName = string.format('ARMY_%d', index)
            local marker = ScenarioUtils.GetMarker(markerName)
            local ax, az
            if marker ~= nil then
                ax, az = marker.position[1], marker.position[3]
            else
                ax, az = sx, sz
            end

            -- Each army's block sits on its own start marker, so the blocks do
            -- not overlap and the armies have to cross the map to engage.
            LOG(string.format('UISTRESS army %d marker %.0f,%.0f', index, ax, az))
            local bx, bz = FindSpot(ax, az, sx, sz, 20)
            LOG(string.format('UISTRESS army %d spot=%s', index, tostring(bx)))
            if bx == nil then
                LOG(string.format('UISTRESS no land for army %d', index))
            else
                local count = STRESS_TANKS
                local made = SpawnBlock(army, bx, bz, count)
                spawnedTotal = spawnedTotal + made
                armiesSpawned = armiesSpawned + 1
                blocks[i] = { x = bx, z = bz, army = army, index = index }

                local fx, fz = FindSpot(ax, az, sx, sz, 30, bx, bz, 40)
                if fx ~= nil then
                    local factory = CreateUnitHPR(Factory, army, fx, GetTerrainHeight(fx, fz), fz, 0, 0, 0)
                    IssueBuildFactory({ factory }, Tank, 200)
                end
            end
        end

        LOG(string.format(
            'UISTRESS spawned tanks=%d (perArmy=%d armies=%d attack=%d)',
            spawnedTotal, STRESS_TANKS, armiesSpawned, STRESS_ATTACK))

        -- Send every army at a different army's block. Units acquire targets
        -- and fire on their own once in range, so no AI brain is required.
        -- `GetUnitsInRect` is the only rect query this build exposes; the
        -- per-army one (`GetArmyUnitsInRect`) does not exist. Each block is its
        -- own rectangle, so the result is that army's units in practice.
        if STRESS_ATTACK ~= 0 and armiesSpawned > 1 then
            for i = 1, armyCount do
                local mine = blocks[i]
                if mine ~= nil then
                    local target = blocks[math.mod(i, armiesSpawned) + 1] or blocks[1]
                    if target ~= nil and target ~= mine then
                        local half = 60
                        local units = GetUnitsInRect(
                            mine.x - half, mine.z - half, mine.x + half, mine.z + half)
                        if units ~= nil then
                            IssueMove(units, { target.x, GetTerrainHeight(target.x, target.z), target.z })
                            LOG(string.format('UISTRESS army %d attacks (%.0f,%.0f)', mine.index, target.x, target.z))
                        end
                    end
                end
            end
        end
    end)
end
