--Misc 1 (ReSkate Misc 1 slot mod): a free accessory slot with its own body slot, so its item
--layers with whatever else is worn. A Misc 1 item's mesh names its material "misc1_mat".
local function createMisc1Item(targetSlot, blueprint, asPreset)
    local misc1Item = createItem("Misc1Item", targetSlot, 0)
    misc1Item:addMeshLodPair(0, blueprint, true)
    
    local material = misc1Item:addMaterial("misc1_mat")
    addEsShaderPreset(misc1Item, material, asPreset)
    return misc1Item
end
local function loadMisc1SlotAsync( recipe)
    if (recipe.assets.Misc1 == nil or Slots.Misc1 == nil) then
        return nil --the template or the loader slot list does not have Misc 1
    end
    if (recipe.assets.Misc1.IsEmpty) then
        return nil --nothing in Misc 1
    end
    
    local blueprintFuture = loadBrtAsync(recipe.assets.Misc1.ObjectBlueprint)
    local appearanceShaderPresetFuture = loadBrtAsync(recipe.assets.Misc1.AppearanceShaderPreset)
    
    return futureCall(
        createMisc1Item,
        Slots.Misc1,
        blueprintFuture,
        appearanceShaderPresetFuture)
end
