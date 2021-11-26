#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;


string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename SearchPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, SearchPredicate search_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, search_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    vector<Document> FindTopDocuments(const string& raw_query,  DocumentStatus input_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [input_status](int document_id, 
            DocumentStatus status, int rating) { return status == input_status; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename SearchPredicate>
    vector<Document> FindAllDocuments(const Query& query, SearchPredicate search_predicate) const {
        map<int, double> document_to_relevance;
        DocumentData map_selection;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                map_selection = documents_.at(document_id);
                if (search_predicate(document_id, map_selection.status, map_selection.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

ostream &operator<<(ostream &os, DocumentStatus status) {
  return os << static_cast<int>(status);
}

template <typename Element , typename Element2>
ostream& operator<<(ostream& out, const map<Element, Element2>& container) {
    int size = 0;
    for (auto [element_1, element_2] : container) {
        ++size;
        if (size == container.size()) {
            out << element_1 << ": " << element_2;
        } else {
            out << element_1 << ": " << element_2 << ", ";
        }
        
    }
    return out;
} 

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    int size = 0;
    for (const Element& element : container) {
        ++size;
        if (size == container.size()) {
            out << element;
        } else {
            out << element << ", "s;
        }
    }
    return out;
} 

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    int size = 0;
    for (const Element& element : container) {
        ++size;
        if (size == container.size()) {
            out << element;
        } else {
            out << element << ", "s;
        }
    }
    return out;
} 

template <typename Func>
void RunTestImpl(Func& func, const string funct_string) {
    func();
    cout << funct_string << " OK" << endl;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

void AssertAlmostEqualImpl(double lhs, const string &lhs_expr_str,
                       double rhs, const string &rhs_expr_str,
                       double precision, const string &file,
                       const string &func, unsigned line,
                       const string &hint) {
  bool almost_equal = abs(lhs - rhs) < precision;
  if (!almost_equal) {
    cout << file << "("s << line << "): "s << func << ": "s;
    cout << lhs_expr_str << " = "s << lhs
         << " is not equal to "s
         << rhs_expr_str << " = "s << rhs
         << " with precision "s << precision;
    if (!hint.empty()) {
      cout << " Hint: "s << hint;
    }
    cout << endl;
    abort();
  }
}
 
#define RUN_TEST(func) RunTestImpl((func), #func)
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT_ALMOST_EQUAL(a, b, precision) AssertAlmostEqualImpl((a), (#a), (b), (#b), (precision),  __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_ALMOST_EQUAL_HINT(a, b, precision, hint) AssertAlmostEqualImpl((a), (#a), (b), (#b), (precision),  __FILE__, __FUNCTION__, __LINE__, (hint))

void TestExcludeStopWordsFromAddedDocumentContent() {
  const int             doc_id                  = 42;
  const string          content                 = "cat in the city"s;
  const vector<int>     ratings                 = {1, 2, 3};
    {
      SearchServer server;
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
      const auto found_docs = server.FindTopDocuments("in"s);
      ASSERT_EQUAL(found_docs.size(), 1u);
      const Document& doc0 = found_docs[0];
      ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
      SearchServer server;
      server.SetStopWords("in the"s);
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
      ASSERT_HINT(server.FindTopDocuments("in"s).empty(), \
            "Stop words must be excluded from documents"s);
    }
}

void TestQueryMatchesAndFindPlusWords() {
  const int             doc_id                  = 42;
  const string          content                 = "cat in the city"s;
  const vector<int>     ratings                 = {1, 2, 3};
  const string          query                   = "cat in"s;
  const vector<string>  expected_matched_words  = { "cat"s, "in"s };
    {
      SearchServer server;
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
     
      auto [matched_words, status] = server.MatchDocument(query, doc_id);
      vector<Document> results = server.FindTopDocuments(query);

      ASSERT_EQUAL(DocumentStatus::ACTUAL, status);
      ASSERT_EQUAL(expected_matched_words, matched_words);
      ASSERT_EQUAL(doc_id, results[0].id);
      ASSERT_EQUAL(ratings[1], results[0].rating);
      ASSERT(results.size());
      ASSERT(results[0].relevance < 1e-6);
    }
}

void TestQueryDoesntMatchAndDoesntFindMinusWords() {
  const int             doc_id                  = 42;
  const string          content                 = "cat in the city"s;
  const vector<int>     ratings                 = {1, 2, 3};
  const string          query                   = "in -cat"s;
    {
      SearchServer server;
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

      auto [matched_words, status] = server.MatchDocument(query, doc_id);
      auto results = server.FindTopDocuments(query);
      ASSERT(results.empty());
      ASSERT(matched_words.empty());
    }
}

void TestStopWordsChangeAffectsSearchResults() {
  const int             doc_id                  = 42;
  const string          content                 = "cat in the city"s;
  const vector<int>     ratings                 = {1, 2, 3};
  const string          query                   = "in"s;
    {
      SearchServer server;
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

      auto results = server.FindTopDocuments(query);
      ASSERT(results.size());
      server.SetStopWords(query);
      results = server.FindTopDocuments(query);
      ASSERT(results.empty());
    }
}

void TestStopWordsChangeAffectsMatchResults() {
  const int             doc_id                  = 42;
  const string          content                 = "cat in the city"s;
  const vector<int>     ratings                 = {1, 2, 3};
  const string          query                   = "city"s;
  const vector<string>  expect                  = { "city"s };
    {
      SearchServer server;
      server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        {
          auto [match, status] = server.MatchDocument(query, doc_id);
          ASSERT_EQUAL(expect, match);
        }
       
        {
          server.SetStopWords(query);
          auto [match, status] = server.MatchDocument(query, doc_id);
          ASSERT(match.empty());
        }
    }
}

void TestAddDocument() {

    const int             doc_id                  = 42;
    const string          content                 = "cat in the city"s;
    const vector<int>     ratings                 = {1, 2, 3};
    const string          query                   = "city"s;
    const vector<string>  expect                  = { "city"s };
    const DocumentStatus  status                  = DocumentStatus::BANNED;
      {
        SearchServer server;
        server.AddDocument(doc_id, content, status, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        {
          int rating_sum = 0;
          for (const int rating : ratings) {
              rating_sum += rating;
          }
          const int averager_rating = static_cast<double>(rating_sum / ratings.size());
          auto documents = server.FindTopDocuments(query, status);
          ASSERT(documents.size());
          ASSERT_EQUAL(documents.at(0).id, doc_id);
          ASSERT_EQUAL(documents.at(0).rating, averager_rating);
        }
        {
          auto [words, matched_status] = server.MatchDocument(query, doc_id);
          ASSERT_EQUAL(status, matched_status);
        }
      }
}

void TestSearchServer() {
    RUN_TEST( TestExcludeStopWordsFromAddedDocumentContent  );
    RUN_TEST( TestQueryMatchesAndFindPlusWords              ); 
    RUN_TEST( TestQueryDoesntMatchAndDoesntFindMinusWords   );
    RUN_TEST( TestStopWordsChangeAffectsSearchResults       );
    RUN_TEST( TestStopWordsChangeAffectsMatchResults        );
    RUN_TEST( TestAddDocument                               );
}

int main() {

    TestSearchServer();

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
} 
