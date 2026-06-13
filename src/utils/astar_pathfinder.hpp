#pragma once
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace utils {

struct PathNode {
    uint32_t x;
    uint32_t y;
    float g_cost; 
    float h_cost; 
    float f_cost() const { return g_cost + h_cost; }
    
    PathNode* parent = nullptr;
    
    PathNode(uint32_t x_, uint32_t y_, float g = 0.0f, float h = 0.0f)
        : x(x_), y(y_), g_cost(g), h_cost(h) {}
        
    bool operator==(const PathNode& other) const {
        return x == other.x && y == other.y;
    }
};

struct PathNodeHash {
    size_t operator()(const PathNode& node) const {
        return (static_cast<size_t>(node.x) << 32) | static_cast<size_t>(node.y);
    }
};

struct PathNodeCompare {
    bool operator()(const PathNode* a, const PathNode* b) const {
        return a->f_cost() > b->f_cost(); 
    }
};

class AStarPathfinder {
public:
    AStarPathfinder(uint32_t width, uint32_t height) 
        : world_width_(width), world_height_(height) {
        collision_grid_.resize(width * height, 0);
    }
    
    
    
    void set_collision(uint32_t x, uint32_t y, uint8_t collision_type) {
        if (x < world_width_ && y < world_height_) {
            collision_grid_[y * world_width_ + x] = collision_type;
        }
    }
    
    
    bool is_blocked(uint32_t x, uint32_t y, bool has_access) const {
        if (x >= world_width_ || y >= world_height_) {
            return true;
        }
        
        uint8_t collision_type = collision_grid_[y * world_width_ + x];
        
        
        if (collision_type == 1 || collision_type == 6) {
            return true;
        }
        
        
        if (collision_type == 3) {
            return !has_access;
        }
        
        
        return false;
    }
    
    
    static float heuristic(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
        return static_cast<float>(std::abs(static_cast<int>(x2) - static_cast<int>(x1)) + 
                                   std::abs(static_cast<int>(y2) - static_cast<int>(y1)));
    }
    
    
    std::vector<PathNode> find_path(uint32_t start_x, uint32_t start_y, 
                                     uint32_t end_x, uint32_t end_y, 
                                     bool has_access) {
        
        if (is_blocked(start_x, start_y, has_access) || is_blocked(end_x, end_y, has_access)) {
            return {}; 
        }
        
        
        if (start_x == end_x && start_y == end_y) {
            return {};
        }
        
        std::priority_queue<PathNode*, std::vector<PathNode*>, PathNodeCompare> open_set;
        std::unordered_map<uint64_t, PathNode*> all_nodes;
        std::unordered_map<uint64_t, bool> closed_set;
        
        auto get_key = [](uint32_t x, uint32_t y) -> uint64_t {
            return (static_cast<uint64_t>(y) << 32) | static_cast<uint64_t>(x);
        };
        
        
        PathNode* start_node = new PathNode(start_x, start_y, 0.0f, 
                                            heuristic(start_x, start_y, end_x, end_y));
        all_nodes[get_key(start_x, start_y)] = start_node;
        open_set.push(start_node);
        
        std::vector<PathNode> result_path;
        bool found = false;
        
        
        const int dx[] = {0, 0, -1, 1};
        const int dy[] = {-1, 1, 0, 0};
        
        while (!open_set.empty()) {
            PathNode* current = open_set.top();
            open_set.pop();
            
            uint64_t current_key = get_key(current->x, current->y);
            
            
            if (closed_set[current_key]) {
                continue;
            }
            
            closed_set[current_key] = true;
            
            
            if (current->x == end_x && current->y == end_y) {
                
                PathNode* path_node = current;
                while (path_node != nullptr) {
                    result_path.push_back(PathNode(path_node->x, path_node->y));
                    path_node = path_node->parent;
                }
                std::reverse(result_path.begin(), result_path.end());
                found = true;
                break;
            }
            
            
            for (int i = 0; i < 4; ++i) {
                int nx = static_cast<int>(current->x) + dx[i];
                int ny = static_cast<int>(current->y) + dy[i];
                
                
                if (nx < 0 || ny < 0 || 
                    nx >= static_cast<int>(world_width_) || 
                    ny >= static_cast<int>(world_height_)) {
                    continue;
                }
                
                uint32_t neighbor_x = static_cast<uint32_t>(nx);
                uint32_t neighbor_y = static_cast<uint32_t>(ny);
                uint64_t neighbor_key = get_key(neighbor_x, neighbor_y);
                
                
                if (is_blocked(neighbor_x, neighbor_y, has_access) || closed_set[neighbor_key]) {
                    continue;
                }
                
                float new_g_cost = current->g_cost + 1.0f;
                
                PathNode* neighbor = all_nodes[neighbor_key];
                if (neighbor == nullptr) {
                    
                    neighbor = new PathNode(neighbor_x, neighbor_y, new_g_cost,
                                           heuristic(neighbor_x, neighbor_y, end_x, end_y));
                    neighbor->parent = current;
                    all_nodes[neighbor_key] = neighbor;
                    open_set.push(neighbor);
                } else if (new_g_cost < neighbor->g_cost) {
                    
                    neighbor->g_cost = new_g_cost;
                    neighbor->parent = current;
                    open_set.push(neighbor);
                }
            }
        }
        
        
        for (auto& pair : all_nodes) {
            delete pair.second;
        }
        
        return result_path;
    }
    
    uint32_t get_width() const { return world_width_; }
    uint32_t get_height() const { return world_height_; }
    
private:
    uint32_t world_width_;
    uint32_t world_height_;
    std::vector<uint8_t> collision_grid_;
};

} 
