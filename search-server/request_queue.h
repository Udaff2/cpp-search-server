#pragma once
#include <deque>
#include "search_server.h"
#include "document.h"

const int MIN_IN_DAY = 1440;

class RequestQueue {
public:

    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string_view raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string_view raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string_view raw_query);
    int GetNoResultRequests() const;

private:
    const SearchServer& search_server_;
    struct QueryResult {
        int results;
    };
    int empty_count_;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = MIN_IN_DAY;

    void RequestsCount(int result);

};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string_view raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
    RequestsCount(result.size());
    return result;
}