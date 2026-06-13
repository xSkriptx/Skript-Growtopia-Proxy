#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace pathfinding {

struct Node {
    uint32_t x = 0;
    uint32_t y = 0;
    float g = 0.0f;  
    float h = 0.0f;  
    float f = 0.0f;  
    Node* parent = nullptr;
    
    Node() = default;
    Node(uint32_t x_, uint32_t y_) : x(x_), y(y_) {}
};

struct NodeCompare {
    bool operator()(const Node* a, const Node* b) const {
        return a->f > b->f;  
    }
};

class AStar {
public:
    AStar(uint32_t width, uint32_t height) 
        : width_(width), height_(height) {
        
        collision_grid_.resize(width * height, 0);
    }
    
    
    void set_collision_data(const std::vector<uint8_t>& collision_data) {
        if (collision_data.size() == collision_grid_.size()) {
            collision_grid_ = collision_data;
            spdlog::debug("[AStar] Loaded collision data: {} tiles", collision_data.size());
        }
    }
    
    
    void set_collision(uint32_t x, uint32_t y, uint8_t collision_type) {
        if (x < width_ && y < height_) {
            collision_grid_[y * width_ + x] = collision_type;
        }
    }
    
    
    uint8_t get_collision(uint32_t x, uint32_t y) const {
        if (x >= width_ || y >= height_) return 1;  
        return collision_grid_[y * width_ + x];
    }
    
    
    
    
    bool is_blocked(uint32_t x, uint32_t y, bool has_access) const {
        uint8_t collision_type = get_collision(x, y);
        
        if (collision_type == 0) {
            return false;
        }
        
        
        if (collision_type == 3) {
            return !has_access;
        }
        
        
        return true;
    }
    
    
    std::vector<Node> find_path(uint32_t start_x, uint32_t start_y, 
                                 uint32_t goal_x, uint32_t goal_y, 
                                 bool has_access) {
        spdlog::debug("[AStar] Finding path from ({},{}) to ({},{}) with access={}",
                     start_x, start_y, goal_x, goal_y, has_access);
        
        
        if (is_blocked(start_x, start_y, has_access)) {
            spdlog::warn("[AStar] Start position is blocked!");
            return {};
        }
        
        if (is_blocked(goal_x, goal_y, has_access)) {
            spdlog::warn("[AStar] Goal position is blocked!");
            return {};
        }
        
        
        if (start_x == goal_x && start_y == goal_y) {
            return {{goal_x, goal_y}};
        }
        
        
        std::priority_queue<Node*, std::vector<Node*>, NodeCompare> open_set;
        
        
        std::unordered_map<uint64_t, Node*> all_nodes;
        std::unordered_map<uint64_t, bool> closed_set;
        
        
        Node* start = new Node(start_x, start_y);
        start->g = 0;
        
        start->h = octile_distance(start_x, start_y, goal_x, goal_y);
        start->f = start->h;
        start->parent = nullptr;
        
        uint64_t start_key = make_key(start_x, start_y);
        all_nodes[start_key] = start;
        open_set.push(start);
        
        Node* goal_node = nullptr;
        int iterations = 0;
        const int max_iterations = 10000;
        
        while (!open_set.empty() && iterations++ < max_iterations) {
            Node* current = open_set.top();
            open_set.pop();
            
            uint64_t current_key = make_key(current->x, current->y);
            
            
            if (closed_set[current_key]) {
                continue;
            }
            
            
            closed_set[current_key] = true;
            
            
            if (current->x == goal_x && current->y == goal_y) {
                goal_node = current;
                break;
            }
            
            
            const int dx4[] = {0, 0, -1, 1};
            const int dy4[] = {-1, 1, 0, 0};
            
            for (int i = 0; i < 4; ++i) {
                int nx = static_cast<int>(current->x) + dx4[i];
                int ny = static_cast<int>(current->y) + dy4[i];
                
                
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(width_) || ny >= static_cast<int>(height_)) {
                    continue;
                }
                
                uint32_t neighbor_x = static_cast<uint32_t>(nx);
                uint32_t neighbor_y = static_cast<uint32_t>(ny);
                
                
                if (is_blocked(neighbor_x, neighbor_y, has_access)) {
                    continue;
                }
                
                uint64_t neighbor_key = make_key(neighbor_x, neighbor_y);
                
                
                if (closed_set[neighbor_key]) {
                    continue;
                }
                
                float new_g = current->g + 1.0f; 
                
                
                auto it = all_nodes.find(neighbor_key);
                if (it != all_nodes.end()) {
                    Node* existing = it->second;
                    if (new_g < existing->g) {
                        existing->g = new_g;
                        existing->f = existing->g + existing->h;
                        existing->parent = current;
                        open_set.push(existing);
                    }
                } else {
                    Node* neighbor = new Node(neighbor_x, neighbor_y);
                    neighbor->g = new_g;
                    neighbor->h = manhattan_distance(neighbor_x, neighbor_y, goal_x, goal_y);
                    neighbor->f = neighbor->g + neighbor->h;
                    neighbor->parent = current;
                    
                    all_nodes[neighbor_key] = neighbor;
                    open_set.push(neighbor);
                }
            }

            
            const int dx8[] = {-1, -1, 1, 1};
            const int dy8[] = {-1, 1, -1, 1};
            
            for (int i = 0; i < 4; ++i) {
                int nx = static_cast<int>(current->x) + dx8[i];
                int ny = static_cast<int>(current->y) + dy8[i];

                
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(width_) || ny >= static_cast<int>(height_)) {
                    continue;
                }

                uint32_t neighbor_x = static_cast<uint32_t>(nx);
                uint32_t neighbor_y = static_cast<uint32_t>(ny);

                
                if (is_blocked(neighbor_x, neighbor_y, has_access)) {
                    continue;
                }

                
                uint32_t adj1x = static_cast<uint32_t>(current->x + dx8[i]);
                uint32_t adj1y = static_cast<uint32_t>(current->y);
                uint32_t adj2x = static_cast<uint32_t>(current->x);
                uint32_t adj2y = static_cast<uint32_t>(current->y + dy8[i]);
                if (is_blocked(adj1x, adj1y, has_access) || is_blocked(adj2x, adj2y, has_access)) {
                    continue;
                }

                uint64_t neighbor_key = make_key(neighbor_x, neighbor_y);
                if (closed_set[neighbor_key]) {
                    continue;
                }

                float new_g = current->g + 1.41421f; 

                auto it = all_nodes.find(neighbor_key);
                if (it != all_nodes.end()) {
                    Node* existing = it->second;
                    if (new_g < existing->g) {
                        existing->g = new_g;
                        existing->f = existing->g + existing->h;
                        existing->parent = current;
                        open_set.push(existing);
                    }
                } else {
                    Node* neighbor = new Node(neighbor_x, neighbor_y);
                    neighbor->g = new_g;
                    
                    neighbor->h = octile_distance(neighbor_x, neighbor_y, goal_x, goal_y);
                    neighbor->f = neighbor->g + neighbor->h;
                    neighbor->parent = current;

                    all_nodes[neighbor_key] = neighbor;
                    open_set.push(neighbor);
                }
            }
        }
        
        
        std::vector<Node> path;
        if (goal_node) {
            Node* current = goal_node;
            while (current) {
                path.push_back(*current);
                current = current->parent;
            }
            std::reverse(path.begin(), path.end());
            spdlog::info("[AStar] Found path with {} nodes", path.size());
        } else {
            spdlog::warn("[AStar] No path found after {} iterations", iterations);
        }
        
        
        for (auto& pair : all_nodes) {
            delete pair.second;
        }
        
        return path;
    }
    
private:
    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> collision_grid_;
    
    
    float manhattan_distance(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) const {
        return static_cast<float>(std::abs(static_cast<int>(x2) - static_cast<int>(x1)) + 
                                 std::abs(static_cast<int>(y2) - static_cast<int>(y1)));
    }

    
    float octile_distance(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) const {
        float dx = static_cast<float>(std::abs(static_cast<int>(x2) - static_cast<int>(x1)));
        float dy = static_cast<float>(std::abs(static_cast<int>(y2) - static_cast<int>(y1)));
        const float F = 1.41421356f; 
        return (dx < dy) ? (F * dx + (dy - dx)) : (F * dy + (dx - dy));
    }
    
    
    uint64_t make_key(uint32_t x, uint32_t y) const {
        return (static_cast<uint64_t>(y) << 32) | x;
    }
};

} 
