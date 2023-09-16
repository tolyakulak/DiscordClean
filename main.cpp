#include <curl/curl.h>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>
#include <array>

using json = nlohmann::json;

std::string getHeader(const std::string headers, const std::string target);
size_t writeToStringCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
int makeRequestUntilSucceed(CURL* curl, std::string& respBody, std::string& respHeaders, long success_code);

int main() {
    std::cout << "*******************************************************************\n"
              << "*             You may be banned for using this tool.              *\n"
              << "*                       Email just for fun:                       *\n"
              << "* pcbe6uy4sxbxhgpna2xyu9hscufpxk5jcb44amzqbm8ttxq6jd@mail2tor.com *\n"
              << "*******************************************************************\n\n";

    CURL* curl;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        // something went wrong
        std::cerr << "Error initializing curl\n";
        curl_global_cleanup();
        return -1;
    }

    std::string TOKEN; // discord authorization token
    long long GUILD_ID; // discord server id
    long long MY_ID; // discord user id

    std::cout << "Enter your authorization token: ";
    std::cin >> TOKEN;
    std::cout << "Enter server id: ";
    std::cin >> GUILD_ID;
    std::cout << "Enter your user id: ";
    std::cin >> MY_ID;
    std::cout << '\n';

    // headers with authorization token
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, std::format("Authorization: {}", TOKEN).c_str());

    // strings for acessing response header and body
    std::string respHeaders;
    std::string respBody;

    /***********************************
     *  Step 1. Collect your messages  *
     ***********************************/

    // setup curl options
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &respHeaders);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeToStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respBody);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToStringCallback);

    // vector for messages id's and channel id's
    std::vector<std::array<long long, 2>> messagesArray;
    // messages saved (used for offset in API requests)
    int index = 0;
    json j;

    // a loop to collect all messages 
    do {
        curl_easy_setopt(curl, CURLOPT_URL, std::format("https://discord.com/api/v10/guilds/{}/messages/search?author_id={}&offset={}", GUILD_ID, MY_ID, index).c_str());
        if (makeRequestUntilSucceed(curl, respBody, respHeaders, 200) == 401) {
            curl_global_cleanup();
            exit(1); // idk if someone trying to change TOKEN in runtime
                       // by using some kind of debugger we should handle 401 error (invalid authorization)
        }

        // parse response as json
        try {
            j = json::parse(respBody);
        }
        catch (json::parse_error& e) {
            std::cerr << "Parse error: " << e.what() << '\n';
            continue; // try again lmao
        }
        // int for totalMessages only for progress bar
        unsigned int totalMessages = j["total_results"];

        // iterate over json data
        for (int i = 0; i < j["messages"].size(); i++) {
            // save message id's to memory
            messagesArray.push_back({ std::stoll(static_cast<std::string>(j["messages"][i][0]["id"])), std::stoll(static_cast<std::string>(j["messages"][i][0]["channel_id"])) });
            index++; // +1 loaded message

            // fancy progress bar!!!
            std::cout << "Loading messages... " << index << "/" << totalMessages << "\r";
        }
        
    } while (!j["messages"].empty());
    std::cout << '\n'; // new line because i used \r in the progress bar

    /*****************************
     *  Step 2. Delete messages  *
     *****************************/

    // reset curl handler
    curl_easy_reset(curl);

    // setup for deleting
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &respHeaders);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeToStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respBody);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToStringCallback);

    // iterate over vector of messages
    for (int i = 0; i < messagesArray.size(); i++) {
        // fancy progress bar!!!
        std::cout << "Deleting messages... " << i << "/" << messagesArray.size() << "\r";

        // set url to delete message by channel_id and message_id
        curl_easy_setopt(curl, CURLOPT_URL, std::format("https://discord.com/api/v10/channels/{}/messages/{}", messagesArray[i][1], messagesArray[i][0]).c_str());
        if (makeRequestUntilSucceed(curl, respBody, respHeaders, 204 /* <- success status code for message deletion is 204 */) == 401) {
            curl_global_cleanup();
            exit(1); // return 1;
        }
    }
    
    /******************************
     *  Step 3. User is happy :)  *
     ******************************/
    std::cout << "\nDone. Good bye!\n";

    // cleanup curl
    curl_global_cleanup();

    return 0;
}

// write data from curl to a std::string
size_t writeToStringCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    ((std::string*)userdata)->append((char*)ptr, nmemb);
    return size * nmemb;
}
// Find and extract a specific header value
// target argument must include ": " in the end for http headers
std::string getHeader(const std::string headers, const std::string target) {
    size_t start = headers.find(target); // start of header name
    if (start != std::string::npos) { // check for failure
        start += target.length();
        // start now is at start of header value instead of header name
        size_t end = headers.find("\r\n", start); // end of header value
        if (end != std::string::npos) { // check for failure again
            return headers.substr(start, end - start); // return header value
        }
    }

    return ""; // return empty on fail;
}

// permorm request (with all options set) until successfull
// returns status code
int makeRequestUntilSucceed(CURL* curl, std::string& respBody, std::string& respHeaders, long success_code) {
    long response_code;
    CURLcode res;
    int tryes = 0;
    do {
        respBody.clear();
        respHeaders.clear();
        
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 429) {
                // 429 == rate limit
                std::cout << "You are being rate limited. ";
                std::chrono::seconds waitDuration;
                try {
                    waitDuration = std::chrono::seconds(std::stoi(getHeader(respHeaders, "retry-after: ")) + 1);
                }
                catch(std::exception& e) {
                    // if getHeader() returned empty string then wait for 2 seconds
                    std::cerr << "Exception: " << e.what() << '\n';
                    waitDuration = std::chrono::seconds(2);
                }
                std::cout << "Waiting " << waitDuration.count() << " seconds before continuing\n";
                std::this_thread::sleep_for(waitDuration);  // sleep
            }
            else if (response_code == 401) {
                // 401 == invalid authorization token
                std::cout << "Invalid authorization token!\n";
                return response_code;
            }
        }
        else {
            std::cerr << "curl_easy_perform() error: " << curl_easy_strerror(res) << '\n';
            if (++tryes >= 10) {
                // if curl_easy_perform() fails 10 times then quit program
                std::cout << "curl_easy_perform() failed 10 times in a row; quiting...\n";
                curl_global_cleanup(); // cleanup before quiting
                exit(1);
            }
        }
    } while (!(res == CURLE_OK && response_code == success_code));

    return success_code;
}
