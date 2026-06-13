-- GrowScan floating items scanner
-- This function is called by the C++ GrowScan command

function scan_floating_items()
    local items = {}
    
    -- GetWorldObject() is provided by C++ and returns all live floating objects
    for _, obj in pairs(GetWorldObject()) do
        if items[obj.id] == nil then
            items[obj.id] = {id = obj.id, count = obj.amount}
        else
            items[obj.id].count = items[obj.id].count + obj.amount
        end
    end
    
    -- Build result string: "itemID,count,itemID,count,..."
    local dwi = ""
    for _, item in pairs(items) do
        dwi = dwi .. item.id .. "," .. item.count .. ","
    end
    
    return dwi
end

log("GrowScan Lua script loaded!")
