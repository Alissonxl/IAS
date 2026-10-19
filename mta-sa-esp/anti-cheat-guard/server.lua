local config = AntiCheatConfig

local function finiteNumber(value)
    return type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge
end

local function validateConfig()
    assert(type(config) == "table", "AntiCheatConfig must be a table")
    assert(finiteNumber(config.reviewThreshold) and config.reviewThreshold > 0,
        "reviewThreshold must be a positive finite number")
    assert(finiteNumber(config.resetThreshold) and config.resetThreshold >= 0
        and config.resetThreshold < config.reviewThreshold,
        "resetThreshold must be non-negative and lower than reviewThreshold")
    assert(finiteNumber(config.scoreDecayPerMinute) and config.scoreDecayPerMinute >= 0,
        "scoreDecayPerMinute must be a non-negative finite number")
    assert(finiteNumber(config.nativeAcDefaultPoints) and config.nativeAcDefaultPoints >= 0,
        "nativeAcDefaultPoints must be a non-negative finite number")
    assert(finiteNumber(config.telemetryWindowMs) and config.telemetryWindowMs >= 1000
        and config.telemetryWindowMs == math.floor(config.telemetryWindowMs),
        "telemetryWindowMs must be an integer of at least 1000")
    assert(type(config.nativeAcWeights) == "table", "nativeAcWeights must be a table")

    for code, points in pairs(config.nativeAcWeights) do
        assert(finiteNumber(code) and code >= 0 and code == math.floor(code),
            "nativeAcWeights keys must be non-negative integer AC codes")
        assert(finiteNumber(points) and points >= 0,
            "nativeAcWeights values must be non-negative finite numbers")
    end
end

validateConfig()

local playerState = {}
local function logRecord(kind, fields)
    fields.kind = kind
    fields.timestamp = getRealTime().timestamp
    outputServerLog("[mta-esp-guard] " .. toJSON(fields))
end

local function getState(player)
    local state = playerState[player]
    if state then
        return state
    end

    state = {
        score = 0,
        seenAC = {},
        alerted = false,
        telemetry = {
            shotsByWeapon = {},
            reportedPlayerHits = 0,
            damageByWeapon = {},
            damageByBodyPart = {},
            damageEvents = 0,
            damageLoss = 0,
        },
    }
    playerState[player] = state
    return state
end

local function playerName(player)
    if isElement(player) then
        return getPlayerName(player)
    end
    return "<disconnected>"
end

local function addNativeACEvidence(player, code)
    local state = getState(player)
    if state.seenAC[code] then
        logRecord("native_ac_duplicate", {
            player = playerName(player),
            code = code,
            score = state.score,
        })
        return
    end

    state.seenAC[code] = true
    local points = config.nativeAcWeights[code]
    if points == nil then
        points = config.nativeAcDefaultPoints
    end
    state.score = state.score + points
    logRecord("native_ac_evidence", {
        player = playerName(player),
        code = code,
        points = points,
        score = state.score,
    })

    if not state.alerted and state.score >= config.reviewThreshold then
        state.alerted = true
        logRecord("manual_review_threshold", {
            player = playerName(player),
            score = state.score,
            threshold = config.reviewThreshold,
            action = "review_only",
        })
    end
end

local function onPlayerACInfo(detectedACList)
    if type(detectedACList) ~= "table" then
        logRecord("invalid_native_ac_payload", { player = playerName(source) })
        return
    end

    local codes = {}
    local uniqueCodes = {}
    for _, code in ipairs(detectedACList) do
        if finiteNumber(code) and code >= 0 and code == math.floor(code) and not uniqueCodes[code] then
            uniqueCodes[code] = true
            codes[#codes + 1] = code
        end
    end
    table.sort(codes)

    logRecord("native_ac_info", {
        player = playerName(source),
        codes = codes,
    })
    for _, code in ipairs(codes) do
        addNativeACEvidence(source, code)
    end
end

local function onPlayerWeaponFire(weapon, _, _, _, hitElement)
    if not isElement(source) or getElementType(source) ~= "player" or not finiteNumber(weapon) then
        return
    end

    local telemetry = getState(source).telemetry
    telemetry.shotsByWeapon[weapon] = (telemetry.shotsByWeapon[weapon] or 0) + 1
    if isElement(hitElement) and getElementType(hitElement) == "player" and hitElement ~= source then
        telemetry.reportedPlayerHits = telemetry.reportedPlayerHits + 1
    end
end

local function onPlayerDamage(attacker, weapon, bodyPart, loss)
    if not isElement(attacker) or getElementType(attacker) ~= "player" then
        return
    end

    local telemetry = getState(attacker).telemetry
    telemetry.damageEvents = telemetry.damageEvents + 1
    if finiteNumber(weapon) then
        telemetry.damageByWeapon[weapon] = (telemetry.damageByWeapon[weapon] or 0) + 1
    end
    if finiteNumber(bodyPart) then
        telemetry.damageByBodyPart[bodyPart] = (telemetry.damageByBodyPart[bodyPart] or 0) + 1
    end
    if finiteNumber(loss) and loss > 0 then
        telemetry.damageLoss = telemetry.damageLoss + loss
    end
end

local function sortedCounts(counts, keyName, countName)
    local result = {}
    for key, count in pairs(counts) do
        result[#result + 1] = { [keyName] = key, [countName] = count }
    end
    table.sort(result, function(a, b)
        return a[keyName] < b[keyName]
    end)
    return result
end

local function flushTelemetry(player, state)
    local telemetry = state.telemetry
    if next(telemetry.shotsByWeapon) == nil and telemetry.damageEvents == 0 then
        return
    end

    logRecord("combat_telemetry", {
        player = playerName(player),
        ping = isElement(player) and getPlayerPing(player) or false,
        score = state.score,
        shotsByWeapon = sortedCounts(telemetry.shotsByWeapon, "weapon", "shots"),
        reportedPlayerHits = telemetry.reportedPlayerHits,
        observedNonlethalDamageEvents = telemetry.damageEvents,
        observedDamageLossTotal = telemetry.damageLoss,
        damageByWeapon = sortedCounts(telemetry.damageByWeapon, "weapon", "events"),
        damageByBodyPart = sortedCounts(telemetry.damageByBodyPart, "bodyPart", "events"),
    })

    telemetry.shotsByWeapon = {}
    telemetry.reportedPlayerHits = 0
    telemetry.damageByWeapon = {}
    telemetry.damageByBodyPart = {}
    telemetry.damageEvents = 0
    telemetry.damageLoss = 0
end

local function onPlayerJoin()
    getState(source)
end

local function onPlayerQuit()
    local state = playerState[source]
    if state then
        flushTelemetry(source, state)
        playerState[source] = nil
    end
end

local function onResourceStart()
    for _, player in ipairs(getElementsByType("player")) do
        getState(player)
        if not resendPlayerACInfo(player) then
            logRecord("native_ac_resend_failed", { player = playerName(player) })
        end
    end
end

local function periodicMaintenance()
    for player, state in pairs(playerState) do
        flushTelemetry(player, state)
        state.score = math.max(0, state.score - config.scoreDecayPerMinute)
        if state.alerted and state.score < config.resetThreshold then
            state.alerted = false
        end
    end
end

addEventHandler("onPlayerACInfo", root, onPlayerACInfo)
addEventHandler("onPlayerWeaponFire", root, onPlayerWeaponFire)
addEventHandler("onPlayerDamage", root, onPlayerDamage)
addEventHandler("onPlayerJoin", root, onPlayerJoin)
addEventHandler("onPlayerQuit", root, onPlayerQuit)
addEventHandler("onResourceStart", resourceRoot, onResourceStart)
setTimer(periodicMaintenance, config.telemetryWindowMs, 0)
