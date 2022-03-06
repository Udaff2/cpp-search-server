#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

vector<string_view> SplitIntoWordsView(string_view text) {
    vector<string_view> words;
    while (true) {
        size_t space = text.find(' ');
        words.push_back(text.substr(0, space));
        if (space == text.npos) {
            break;
        }
        else {
            text.remove_prefix(space + 1);
        }
    }
    return words;
}