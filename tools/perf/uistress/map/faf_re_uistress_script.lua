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

-- 900 tanks in two blocks: enough to make the strategic-icon and selection
-- passes do real work, small enough that the sim itself stays cheap.
local STRESS_TANKS = 900
local BLOCK_COLUMNS = 30
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

function OnStart(self)
    ForkThread(function()
        local start = ScenarioUtils.GetMarker('ARMY_1').position
        local sx, sz = start[1], start[3]

        local brain = ArmyBrains[1]
        local army = brain.Name
        -- Plenty of resources, so the factory never stalls and the AI-less
        -- army does not need to manage economy during the measurement.
        brain:GiveStorage('Mass', 1000000)
        brain:GiveStorage('Energy', 1000000)
        brain:GiveResource('Mass', 1000000)
        brain:GiveResource('Energy', 1000000)

        local bx, bz = FindSpot(sx, sz, sx, sz, 45)
        if bx == nil then
            LOG('UISTRESS no land for the main block')
            return
        end
        local fx, fz = FindSpot(sx, sz, sx, sz, 60, bx, bz, 60)
        local cx, cz = bx, bz
        if fx ~= nil then
            cx, cz = FindSpot(sx, sz, sx, sz, 60, fx, fz, 60) or bx, bz
        end

        local mainCount = SpawnBlock(army, bx, bz, math.floor(STRESS_TANKS * 0.6))
        local secondCount = SpawnBlock(army, cx, cz, STRESS_TANKS - math.floor(STRESS_TANKS * 0.6))

        local factory = nil
        if fx ~= nil then
            factory = CreateUnitHPR(Factory, army, fx, GetTerrainHeight(fx, fz), fz, 0, 0, 0)
            IssueBuildFactory({ factory }, Tank, 200)
        end

        LOG(string.format(
            'UISTRESS spawned tanks=%d (block A at %.0f,%.0f; block B at %.0f,%.0f) factory=%s',
            mainCount + secondCount, bx, bz, cx, cz, tostring(factory ~= nil)))
    end)
end
