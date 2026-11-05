-- UI stress driver for the Lua incremental-GC performance runs (tools/perf).
--
-- Mounted through a hook folder, so it only runs on the user-side UI VM. It
-- never touches the sim: every call below is a camera or selection API the
-- shipped client itself uses; the sim state and its checksums are untouched.
--
-- What loads the UI VM here is the camera. Zooming between the minimum and the
-- maximum makes the UI re-project every strategic icon, selection bracket and
-- overlay in view on every step, which is where the bulk of the UI Lua
-- allocation in a real game comes from. A selection pass rides along so the
-- selection set itself is rebuilt too.
--
-- It logs "UISTRESS ..." lines so a run can be checked from the game log.

local ZOOM_STEPS_PER_SECOND = 8      -- zoom direction flips at this rate
local SELECT_EVERY = 8               -- rebuild the selection every Nth flip

local UiStressOnFirstUpdate = OnFirstUpdate
function OnFirstUpdate()
    UiStressOnFirstUpdate()

    ForkThread(function()
        WaitSeconds(0.5)

        local cam = GetCamera('WorldCamera')
        if cam == nil then
            LOG('UISTRESS no WorldCamera, aborting')
            return
        end

        -- Start fully zoomed out so the first pass sees everything.
        cam:Reset()
        local minZoom = cam:GetMinZoom()
        local maxZoom = cam:GetMaxZoom()
        LOG(string.format('UISTRESS start minZoom=%.3f maxZoom=%.3f', minZoom, maxZoom))

        local flip = 0
        local lastReport = 0
        local interval = 1.0 / ZOOM_STEPS_PER_SECOND
        local target = maxZoom

        while true do
            -- Alternate between the two extremes. SetZoom with a 0-second
            -- transition is instantaneous, which is what forces the UI to
            -- rebuild rather than interpolate.
            target = (target == maxZoom) and minZoom or maxZoom
            cam:SetZoom(target, 0)

            flip = flip + 1
            if math.mod(flip, SELECT_EVERY) == 0 then
                -- Rebuild the visible selection while the camera is at this
                -- extreme: selection UI plus the icon pass together.
                UISelectionByCategory('LAND', false, false, false, false)
                local selected = GetSelectedUnits() or {}
                if table.getn(selected) == 0 then
                    SelectUnits(nil)
                end
            end

            if flip - lastReport >= 80 then
                lastReport = flip
                LOG(string.format('UISTRESS flips=%d zoom=%.3f', flip, target))
            end

            WaitSeconds(interval)
        end
    end)
end
