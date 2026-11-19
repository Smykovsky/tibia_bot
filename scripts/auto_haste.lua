-- Example custom Lua script for TibiaBot
-- Automatically casts "utani gran hur" (or "utani hur") if player has enough mana

local player = Game.getPlayer()
Bot.log("Running Auto Haste script for player: " .. player.name .. " (MP: " .. player.mp .. ")")

if player.mp >= 100 then
    Bot.castSpell("utani gran hur")
    Bot.log("Haste casted successfully!")
else
    Bot.log("Not enough mana for haste.")
end
