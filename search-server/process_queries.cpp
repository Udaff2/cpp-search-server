#include "process_queries.h"
#include <execution>
#include <functional>

#include <iostream>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries)
{
    vector<vector<Document>> result(queries.size());
    transform(
        execution::par,
        queries.begin(),
        queries.end(),
        result.begin(),
        [&search_server](const string query) {
            return search_server.FindTopDocuments(query);
        }
    );
    return result;    
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
    std::vector<Document> result;

    vector<vector<Document>> documents_lists = ProcessQueries(search_server, queries);
    
    for (const auto documents : documents_lists) {
        for(auto document : documents) {
            result.push_back(document);
        }
    }
    return result;
}