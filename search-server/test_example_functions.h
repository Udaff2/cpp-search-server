#pragma once
#include <vector>
#include <string>
#include "search_server.h"

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view query);
