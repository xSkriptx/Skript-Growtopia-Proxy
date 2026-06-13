-- Simple A* Pathfinding Algorithm
-- Returns a path as a table of {x, y} coordinates

-- Manhattan distance heuristic
function manhattan_distance(x1, y1, x2, y2)
    return math.abs(x2 - x1) + math.abs(y2 - y1)
end

-- A* pathfinding - returns path or nil if no path found
function FindPath(start_x, start_y, end_x, end_y, collision_callback)
    -- collision_callback(x, y) should return true if tile is walkable
    local is_walkable = collision_callback or function(x, y)
        -- Default: assume all tiles walkable
        return x >= 0 and x < 100 and y >= 0 and y < 60
    end
    
    local open_set = {}
    local closed_set = {}
    
    -- Node: {x, y, g, h, f, parent}
    local start_node = {
        x = start_x,
        y = start_y,
        g = 0,
        h = manhattan_distance(start_x, start_y, end_x, end_y),
        f = 0,
        parent = nil
    }
    start_node.f = start_node.g + start_node.h
    
    table.insert(open_set, start_node)
    
    local max_iterations = 5000
    local iterations = 0
    
    while #open_set > 0 and iterations < max_iterations do
        iterations = iterations + 1
        
        -- Find node with lowest f cost
        local current_idx = 1
        for i = 2, #open_set do
            if open_set[i].f < open_set[current_idx].f then
                current_idx = i
            end
        end
        
        local current = table.remove(open_set, current_idx)
        
        -- Found goal
        if current.x == end_x and current.y == end_y then
            local path = {}
            local node = current
            while node do
                table.insert(path, 1, {x = node.x, y = node.y})
                node = node.parent
            end
            return path
        end
        
        -- Mark as visited
        local key = current.x .. "," .. current.y
        closed_set[key] = true
        
        -- Check 4 neighbors (up, down, left, right)
        local neighbors = {
            {current.x, current.y - 1},
            {current.x, current.y + 1},
            {current.x - 1, current.y},
            {current.x + 1, current.y}
        }
        
        for _, pos in ipairs(neighbors) do
            local nx, ny = pos[1], pos[2]
            local nkey = nx .. "," .. ny
            
            if is_walkable(nx, ny) and not closed_set[nkey] then
                local g = current.g + 1
                local h = manhattan_distance(nx, ny, end_x, end_y)
                local f = g + h
                
                -- Check if already in open set
                local found = false
                for i, node in ipairs(open_set) do
                    if node.x == nx and node.y == ny then
                        found = true
                        if g < node.g then
                            node.g = g
                            node.f = f
                            node.parent = current
                        end
                        break
                    end
                end
                
                if not found then
                    table.insert(open_set, {
                        x = nx, y = ny,
                        g = g, h = h, f = f,
                        parent = current
                    })
                end
            end
        end
    end
    
    return nil -- No path found
end

-- Example usage in Growtopia Lua:
-- local path = FindPath(0, 0, 10, 10)
-- if path then
--     for i, node in ipairs(path) do
--         print("Step " .. i .. ": (" .. node.x .. ", " .. node.y .. ")")
--         -- Use Growtopia's own movement functions here
--     end
-- end
