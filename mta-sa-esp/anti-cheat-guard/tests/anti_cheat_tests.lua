local handlers = {}
local timers = {}
local records = {}
local logs = {}
local players = {}
local currentTime = 1700000000
local jsonIndex = 0

root = {}
resourceRoot = {}

function addEventHandler(eventName, attachedTo, callback)
    assert(attachedTo == root or attachedTo == resourceRoot)
    handlers[eventName] = callback
    return true
end

function setTimer(callback, interval, repetitions)
    timers[#timers + 1] = { callback = callback, interval = interval, repetitions = repetitions }
    return timers[#timers]
end

function getElementsByType(elementType)
    assert(elementType == "player")
    return players
end

function isElement(element)
    return type(element) == "table" and element.alive == true
end

function getElementType(element)
    return element.elementType
end

function getPlayerName(player)
    return player.name
end

function getPlayerPing(player)
    return player.ping
end

function getRealTime()
    return { timestamp = currentTime }
end

function toJSON(value)
    jsonIndex = jsonIndex + 1
    local key = "record-" .. jsonIndex
    records[key] = value
    return key
end

function outputServerLog(message)
    logs[#logs + 1] = message
end

function resendPlayerACInfo(player)
    return isElement(player)
end

local function fire(eventName, player, ...)
    source = player
    assert(handlers[eventName], "Missing event handler: " .. eventName)
    handlers[eventName](...)
end

local function recordsOfKind(kind)
    local result = {}
    for _, record in pairs(records) do
        if record.kind == kind then
            result[#result + 1] = record
        end
    end
    return result
end

AntiCheatConfig = {
    reviewThreshold = 25,
    resetThreshold = 10,
    scoreDecayPerMinute = 2,
    nativeAcDefaultPoints = 10,
    nativeAcWeights = { [91] = 7 },
    telemetryWindowMs = 60000,
}

dofile("anti-cheat-guard/server.lua")

local attacker = { alive = true, elementType = "player", name = "TestAttacker", ping = 45 }
local victim = { alive = true, elementType = "player", name = "TestVictim", ping = 60 }
players = { attacker, victim }
fire("onResourceStart", resourceRoot)
assert(#timers == 1 and timers[1].interval == 60000 and timers[1].repetitions == 0)

fire("onPlayerACInfo", attacker, { 42, 91, 42 })
local evidence = recordsOfKind("native_ac_evidence")
assert(#evidence == 2, "Distinct native AC codes should be scored once")
assert(evidence[1].code == 42 or evidence[2].code == 42)
local weighted = evidence[1].code == 91 and evidence[1] or evidence[2]
assert(weighted.points == 7, "Per-code configurable weights should be honored")
assert(#recordsOfKind("manual_review_threshold") == 0, "Score should remain below threshold")

fire("onPlayerACInfo", attacker, { 42 })
assert(#recordsOfKind("native_ac_evidence") == 2, "Repeated AC resend must not inflate score")
assert(#recordsOfKind("native_ac_duplicate") == 1, "Repeated AC code should be logged")

local beforeShots = #recordsOfKind("native_ac_evidence")
for index = 1, 10000 do
    fire("onPlayerWeaponFire", attacker, 24, 10, 20, 30, victim, 1, 2, 3)
end
fire("onPlayerDamage", victim, attacker, 24, 9, 5)
assert(#recordsOfKind("native_ac_evidence") == beforeShots,
    "Combat statistics must not produce cheat score")

timers[1].callback()
local telemetry = recordsOfKind("combat_telemetry")
assert(#telemetry == 1, "Combat events should produce one aggregated record")
assert(telemetry[1].shotsByWeapon[1].weapon == 24 and telemetry[1].shotsByWeapon[1].shots == 10000)
assert(telemetry[1].reportedPlayerHits == 10000)
assert(telemetry[1].observedNonlethalDamageEvents == 1)
assert(telemetry[1].ping == 45)
assert(#recordsOfKind("manual_review_threshold") == 0)

fire("onPlayerACInfo", attacker, { 55, 56 })
assert(#recordsOfKind("manual_review_threshold") == 1, "Threshold should request manual review")
local alert = recordsOfKind("manual_review_threshold")[1]
assert(alert.action == "review_only" and alert.score == 27)

attacker.alive = false
fire("onPlayerQuit", attacker)
attacker.alive = true
fire("onPlayerJoin", attacker)
fire("onPlayerACInfo", attacker, { 42 })
assert(#recordsOfKind("native_ac_evidence") == 5, "A new session should start with fresh AC state")
assert(#logs > 0, "Records should be emitted to the server log")

print("anti-cheat telemetry/scoring tests passed")
