#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <numeric>
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
  set<string> non_empty_strings;
  for (const string& str : strings) {
    if (!str.empty()) {
      non_empty_strings.insert(str);
    }
  }
  return non_empty_strings;
}

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
  Document() = default;

  Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
  }

  int id = 0;
  double relevance = 0.0;
  int rating = 0;
};

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

class SearchServer {
public:

  inline static constexpr int INVALID_DOCUMENT_ID = -1;
  
  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words){
    for (const auto& word : stop_words) {
      if (!IsValidWord(word)) {
        throw invalid_argument("Наличие недопустимых символов в стоп-слове"s);
      }
    }
    stop_words_ = MakeUniqueNonEmptyStrings(stop_words);
  }

  explicit SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
  {
  }

  void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id>=0 && documents_.count(document_id)<1){
      const vector<string> words = SplitIntoWordsNoStop(document);
      const double inv_word_count = 1.0 / words.size();
      for (const string& word : words) {
        if (IsValidWord(word)) {
          word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        else {
          throw invalid_argument("Наличие недопустимых символов в тексте добавляемого документа"s);
        }
      }
      documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    } else {
      if (document_id < 0) {
        throw invalid_argument("Попытка добавить документ с отрицательным id"s);
      }
      else {
        throw invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
      }
    }
  }

  template <typename SearchPredicate>
  vector<Document> FindTopDocuments(const string& raw_query, SearchPredicate search_predicate) const {
    const Query query = ParseQuery(raw_query);

    for (const string& word : query.plus_words) {
      if (!IsValidWord(word)){
        throw invalid_argument("В словах поискового запроса есть недопустимые символы"s);
      }
    }
    for (const string& word : query.minus_words) {
      if (word[0]=='-'){
        throw invalid_argument("Наличие более чем одного минуса перед стоп-словом"s);
      }
      else if (word.empty()) {
        throw invalid_argument("Отсутствие текста после символа «минус»"s);
      }
    }
    auto matched_documents = FindAllDocuments(query, search_predicate);
    auto epsilon = 1e-6;
    sort(matched_documents.begin(), matched_documents.end(), [epsilon](const Document& lhs, const Document& rhs) {
      if (abs(lhs.relevance - rhs.relevance) < epsilon) {
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

  vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
      return document_status == status;
     });
  }

  vector<Document> FindTopDocuments(const string& raw_query) const {
     return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
  }

  int GetDocumentCount() const {
    return documents_.size();
  }

  int GetDocumentId(int index) const {
     if (index > documents_.size()||index < 0){
       throw out_of_range("Индекс переданного документа выходит за пределы допустимого диапазона"s);
       return SearchServer::INVALID_DOCUMENT_ID;
     }
     else return documents_.count(index);
  }

  tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
      if (!IsValidWord(word)){
        throw invalid_argument("В словах поискового запроса есть недопустимые символы"s);
      }
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.push_back(word);
      }
    }
    for (const string& word : query.minus_words) {
      if (word[0]=='-'){
        throw invalid_argument("Наличие более чем одного минуса перед стоп-словом"s);
      }
      else if (word.empty()) {
        throw invalid_argument("Отсутствие текста после символа «минус»"s);
      }
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.clear();
        break;
      }
    }
    return tuple{matched_words, documents_.at(document_id).status};
  }
 void SetStopWords(const string& text) {
    for (const string& word : SplitIntoWords(text)) {
      stop_words_.insert(word);
    }
  }

private:
  
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;

  static bool IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
      return c >= '\0' && c < ' ';
    });
  }
  
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
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
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
    auto &map_select = map_selection;
    for (const string& word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
        map_select = documents_.at(document_id);
        if (search_predicate(document_id, map_select.status, map_select.rating)) {
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
     << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
  cout << "{ "s
     << "document_id = "s << document_id << ", "s
     << "status = "s << static_cast<int>(status) << ", "s
     << "words ="s;
  for (const string& word : words) {
    cout << ' ' << word;
  }
  cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
         const vector<int>& ratings) {
  try {
    search_server.AddDocument(document_id, document, status, ratings);
  } catch (const exception& e) {
    cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
  }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
  cout << "Результаты поиска по запросу: "s << raw_query << endl;
  try {
    for (const Document& document : search_server.FindTopDocuments(raw_query)) {
      PrintDocument(document);
    }
  } catch (const exception& e) {
    cout << "Ошибка поиска: "s << e.what() << endl;
  }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
  try {
    cout << "Матчинг документов по запросу: "s << query << endl;
    const int document_count = search_server.GetDocumentCount();
    for (int index = 0; index < document_count; ++index) {
      const int document_id = search_server.GetDocumentId(index);
      const auto [words, status] = search_server.MatchDocument(query, document_id);
      PrintMatchDocumentResult(document_id, words, status);
    }
  } catch (const exception& e) {
    cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
  }
}

static const vector<string> test_documents               = {{"белый ухоженный кот и модный ошейник"s},
                                                            {"кот и ухоженный пушистый хвост"s},
                                                            {"ухоженный пёс выразительные глаза"s},
                                                            {"ухоженный скворец евгений"s}
                                                           };
static const vector<int> test_id                         = {{0},
                                                            {1},
                                                            {2},
                                                            {3}
                                                           };
static const vector<vector<int>> test_rating            =  {{8, -3},
                                                            {7, 2, 7},
                                                            {5, -12, 2, 1},
                                                            {9}
                                                           };
static const vector<DocumentStatus> test_status         =  {DocumentStatus::ACTUAL,
                                                            DocumentStatus::ACTUAL,
                                                            DocumentStatus::ACTUAL,
                                                            DocumentStatus::BANNED
                                                           };


ostream &operator<<(ostream &os, const DocumentStatus &status) {
  switch(status) {
    case DocumentStatus::ACTUAL:      os << "ACTUAL"s;     break;
    case DocumentStatus::IRRELEVANT:  os << "IRRELEVANT"s; break;
    case DocumentStatus::BANNED:      os << "BANNED"s;     break;
    case DocumentStatus::REMOVED:     os << "REMOVED"s;    break;
  }
  return os;
}

template <typename Element , typename Element2>
ostream& operator<<(ostream& out, const map<Element, Element2>& container) {
  int size = 0;
  auto container_size = container.size();
  for (auto [element_1, element_2] : container) {
    ++size;
    out << element_1 << ": " << element_2;
    if (size != container_size) {
      out << ", ";
    }
  }
  return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    int size = 0;
    auto container_size = container.size();
    for (const Element& element : container) {
        ++size;
        out << element;
        if (size != container_size) {
            out << ", "s;
        }
    }
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    int size = 0;
    auto container_size = container.size();
    for (const Element& element : container) {
        ++size;
        out << element;
        if (size == container_size) {
          out << element << ", "s;
        }
    }
    return out;
}

template <typename Func>
void RunTestImpl(Func& func, const string funct_string) {
    func();
    cerr << funct_string << " OK" << endl;
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

void TestAddDocument() {
  SearchServer server(""s);
  int document_size = 0;
  ASSERT_EQUAL(document_size, server.GetDocumentCount());
  for (int i = 0; i < test_documents.size(); ++i) {
    ++document_size;
    server.AddDocument(test_id[i], test_documents[i], test_status[i], test_rating[i]);
    ASSERT_EQUAL(document_size, server.GetDocumentCount());
  }
}

void TestCalculationAverageRating() {
  SearchServer server(""s);
  const vector<string> query = {{"белый"s}, {"хвост"s}, {"пёс"s}, {"скворец"s}};
  static const vector<string> test_car_documents = {
    {"белый ухоженный кот и модный ошейник"s}, {"кот и ухоженный пушистый хвост"s},
    {"ухоженный пёс выразительные глаза"s}, {"ухоженный скворец евгений"s}
  };
  static const vector<int> test_car_id = {
    {0}, {1},
    {2}, {3}
  };
  static const vector<vector<int>> test_car_rating =  {
    {8, -3}, {7, 2, 7},
    {5, -12, 2, 1}, {9}
  };
  for (int i = 0; i < test_car_documents.size(); ++i) {
    server.AddDocument(test_car_id[i], test_car_documents[i], DocumentStatus::ACTUAL, test_car_rating[i]);
    const int avg_rating = static_cast<double>(accumulate(test_car_rating[i].begin(), test_car_rating[i].end(), 0)) / test_car_rating[i].size();
    auto documents = server.FindTopDocuments(query[i]);
    ASSERT_EQUAL(documents.at(0).rating, avg_rating);
  }
}

void TestMatchDocumentPlusWords() {
  const string query = "ухоженный"s;
  const vector<string> expected_matched_words = { "ухоженный"s};
  SearchServer server(""s);
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], test_status[i], test_rating[i]);
    auto [matched_words, status] = server.MatchDocument(query, test_id[i]);
    ASSERT_EQUAL(test_status[i], status);
    ASSERT_EQUAL(expected_matched_words, matched_words);
  }
}

void TestMatchDocumentMinusWords() {

  SearchServer server(""s);
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], test_status[i], test_rating[i]);
    {
      const string query = "ухоженный"s;
      auto [matched_words, status] = server.MatchDocument(query, test_id[i]);
      ASSERT_EQUAL(test_status[i], status);
      ASSERT_EQUAL(1u, matched_words.size());
    }
    {
      const string query = "белый -кот "s;
      auto [matched_words, status] = server.MatchDocument(query, test_id[i]);
      ASSERT_EQUAL(0u, matched_words.size());
    }
    {
      const string query = "-ухоженный"s;
      auto [matched_words, status] = server.MatchDocument(query, test_id[i]);
      ASSERT(matched_words.empty());
    }
  }
}

void TestMatchDocumentStopWords() {
  const string query = "белый"s;
  const string stop_word = "белый"s;
  SearchServer server(""s);
    for (int i = 0; i < test_documents.size(); ++i) {
      server.AddDocument(test_id[i], test_documents[i], test_status[i], test_rating[i]);
    }
    {
      const auto found_docs = server.FindTopDocuments(query);
      ASSERT_EQUAL(found_docs.size(), 1u);
      const Document& doc0 = found_docs[0];
      ASSERT_EQUAL(doc0.id, 0);
    }
    {
      server.SetStopWords(stop_word);
      ASSERT_HINT(server.FindTopDocuments(query).empty(), "Стоп-слова необходимо исключить из документов"s);
    }
}

void TestFindTopDocumentsPlusWords() {
  const string query = "белый"s;
  SearchServer server(""s);
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], DocumentStatus::ACTUAL, test_rating[i]);
  }
  auto results = server.FindTopDocuments(query);
  ASSERT_EQUAL(1u, results.size());
  ASSERT_EQUAL(0, results.at(0).id);
}

void TestFindTopDocumentsMinusWords() {
  SearchServer server(""s);
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], DocumentStatus::ACTUAL, test_rating[i]);
  }

  {
    const string query = "ухоженный"s;
    auto search_result = server.FindTopDocuments(query);
    ASSERT_EQUAL(4u, search_result.size());
  }
  {
    const string query = "-ухоженный"s;
    auto search_result = server.FindTopDocuments(query);
    ASSERT(search_result.empty());
  }
}

void TestFindTopDocumentsStopWords() {
  SearchServer server(""s);
  const string query = "ухоженный"s;
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], test_status[i], test_rating[i]);
  }
  {
    auto search_result = server.FindTopDocuments(query);
    ASSERT_EQUAL(3u, search_result.size());
  }
  {
    auto search_results = server.FindTopDocuments(query);
    ASSERT_EQUAL(3u, search_results.size());
  }
  {
    server.SetStopWords(query);
    auto search_results = server.FindTopDocuments(query);
    ASSERT(search_results.empty());
  }
}

void TestFindTopDocumentsbyStatus() {
  int test_documents_size = test_documents.size();
  const string query = "ухоженный"s;
  SearchServer server(""s);
  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], DocumentStatus::IRRELEVANT, test_rating[i]);
  }
  {
    auto search_results = server.FindTopDocuments(query);
    ASSERT(search_results.empty());
  }
  {
    auto search_results = server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(test_documents_size, search_results.size());
  }
}

void TestFindTopDocumentsUsingPredicate() {
  SearchServer server(""s);
  const string query = "ухоженный"s;
  for (size_t i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], DocumentStatus::ACTUAL, test_rating[i]);
  }
  {
    const size_t expected_documents_count =
        min(test_documents.size(), static_cast<size_t>(MAX_RESULT_DOCUMENT_COUNT));
    ASSERT_EQUAL(expected_documents_count, server.FindTopDocuments(query).size());
  }
  {
    const int id = 0;
    auto predicate_id = [id](int doc_id, DocumentStatus status, int rating) {
      return doc_id == id;
    };
    auto search_results = server.FindTopDocuments(query, predicate_id);
    ASSERT_EQUAL(1u, search_results.size());
    ASSERT_EQUAL(id, search_results.at(0).id);
  }
  {
    const int id_not_exists = test_documents.size();
    auto predicate_id = [id_not_exists](int doc_id, DocumentStatus status, int rating) {
      return doc_id == id_not_exists;
    };
    auto search_results = server.FindTopDocuments(query, predicate_id);
    ASSERT(search_results.empty());
  }
  {
    int avg_rating;
    vector <int> avg_rating_vector;
    for (int i = 0; i < test_rating.size(); ++i) {
      avg_rating = static_cast<double>(accumulate(test_rating[i].begin(), test_rating[i].end(), 0)) / test_rating[i].size();
      avg_rating_vector.push_back(avg_rating);
    }
    const int test_rating = *max_element( avg_rating_vector.begin(), avg_rating_vector.end() ) + 1;
    auto predicate_rating = [test_rating](int doc_id, DocumentStatus status, int rating) {
      return rating == test_rating;
    };
    auto search_results = server.FindTopDocuments(query, predicate_rating);
    ASSERT(search_results.empty());
  }
}

void TestFindTopDocumentsSort() {
  SearchServer server(""s);
  const string query = "ухоженный"s;


  for (int i = 0; i < test_documents.size(); ++i) {
    server.AddDocument(test_id[i], test_documents[i], DocumentStatus::ACTUAL, test_rating[i]);
  }

  auto search_results = server.FindTopDocuments(query);
  for (size_t i = 1; i < search_results.size(); ++i) {
    bool sort_relevance = false;
    if (abs(search_results.at(i).relevance - search_results.at(i-1).relevance) < 1e-7 ) {
      if(search_results.at(i).rating < search_results.at(i-1).rating) {
        sort_relevance = true;
      }
    }
    else if(search_results.at(i).relevance < search_results.at(i-1).relevance) {
      sort_relevance = true;
    }
    ASSERT_HINT(sort_relevance, "Relevance sorting must be made properly"s);
  }
}

void TestTfIdfCalculation() {
  SearchServer server(""s);
 
  const vector<string> test_documents_tf_idf = { "белый кот и модный ошейник"s,
                                                 "пушистый кот пушистый хвост"s,
                                                 "ухоженный пёс выразительные глаза"s,
                                                 "ухоженный скворец евгений"s};
 
   const size_t test_documents_size = test_documents_tf_idf.size();
   const map<string, map<int, double>> tf {
    { "белый"s,         { { 0, 1.0/static_cast<double>(test_documents_size) }, } },
    { "кот"s,           { { 0, 1.0/static_cast<double>(test_documents_size) },
                          { 1, 1.0/static_cast<double>(test_documents_size) }, } },
    { "и"s,             { { 0, 1.0/static_cast<double>(test_documents_size) }, } },
    { "модный"s,        { { 0, 1.0/static_cast<double>(test_documents_size) }, } },
    { "ошейник"s,       { { 0, 1.0/static_cast<double>(test_documents_size) }, } },
    { "пушистый"s,      { { 1, 1.0/static_cast<double>(test_documents_size) }, } },
    { "хвост"s,         { { 1, 1.0/static_cast<double>(test_documents_size) }, } },
    { "ухоженный"s,     { { 2, 1.0/static_cast<double>(test_documents_size) },
                          { 3, 1.0/static_cast<double>(test_documents_size) }, } },
    { "пёс"s,           { { 2, 1.0/static_cast<double>(test_documents_size) }, } },
    { "выразительные"s, { { 2, 1.0/static_cast<double>(test_documents_size) }, } },
    { "глаза"s,         { { 2, 1.0/static_cast<double>(test_documents_size) }, } },
    { "скворец"s,       { { 3, 1.0/static_cast<double>(test_documents_size) }, } },
    { "евгений"s,       { { 3, 1.0/static_cast<double>(test_documents_size) }, } },
  };
 
  map<string, int> df;
  for (const auto & [term, freqs] : tf) {
    df[term] = freqs.size();
  }
  const map<string, double> idf = {
    { "белый"s,         log( static_cast<double>(test_documents_size)/df.at("белый"s)         ) },
    { "кот"s,           log( static_cast<double>(test_documents_size)/df.at("кот"s)           ) },
    { "и"s,             log( static_cast<double>(test_documents_size)/df.at("и"s)             ) },
    { "модный"s,        log( static_cast<double>(test_documents_size)/df.at("модный"s)        ) },
    { "ошейник"s,       log( static_cast<double>(test_documents_size)/df.at("ошейник"s)       ) },
    { "пушистый"s,      log( static_cast<double>(test_documents_size)/df.at("пушистый"s)      ) },
    { "хвост"s,         log( static_cast<double>(test_documents_size)/df.at("хвост"s)         ) },
    { "ухоженный"s,     log( static_cast<double>(test_documents_size)/df.at("ухоженный"s)     ) },
    { "пёс"s,           log( static_cast<double>(test_documents_size)/df.at("пёс"s)           ) },
    { "выразительные"s, log( static_cast<double>(test_documents_size)/df.at("выразительные"s) ) },
    { "глаза"s,         log( static_cast<double>(test_documents_size)/df.at("глаза"s)         ) },
    { "скворец"s,       log( static_cast<double>(test_documents_size)/df.at("скворец"s)       ) },
    { "евгений"s,       log( static_cast<double>(test_documents_size)/df.at("евгений"s)       ) },
  };
 
  const string queries = "пушистый ухоженный кот"s;
  vector<size_t> size_documents_tf_idf = {};
  for (int i = 0; i < test_documents_tf_idf.size(); ++i) {
      size_documents_tf_idf.push_back(SplitIntoWords(test_documents_tf_idf[i]).size());
  }
 
 map<size_t, map<size_t, double>> query_to_doc_relevance = {
      { 0, {
        { 0, 0.0/static_cast<double>(size_documents_tf_idf[0]) * idf.at("пушистый"s)   },
        { 1, 0.0/static_cast<double>(size_documents_tf_idf[0]) * idf.at("ухоженный"s)  },
        { 2, 1.0/static_cast<double>(size_documents_tf_idf[0]) * idf.at("кот"s)        } },
      },
      { 1, {
        { 0, 2.0/static_cast<double>(size_documents_tf_idf[1]) * idf.at("пушистый"s)   },
        { 1, 0.0/static_cast<double>(size_documents_tf_idf[1]) * idf.at("ухоженный"s)  },
        { 2, 1.0/static_cast<double>(size_documents_tf_idf[1]) * idf.at("кот"s)        } },
      },
      { 2, {
        { 0, 0.0/static_cast<double>(size_documents_tf_idf[2]) * idf.at("пушистый"s)   },
        { 1, 1.0/static_cast<double>(size_documents_tf_idf[2]) * idf.at("ухоженный"s)  },
        { 2, 0.0/static_cast<double>(size_documents_tf_idf[2]) * idf.at("кот"s)        } },
      },
      { 3, {
        { 0, 0.0/static_cast<double>(size_documents_tf_idf[3]) * idf.at("пушистый"s)   },
        { 1, 1.0/static_cast<double>(size_documents_tf_idf[3]) * idf.at("ухоженный"s)  },
        { 2, 0.0/static_cast<double>(size_documents_tf_idf[3]) * idf.at("кот"s)        } },
      }
    };
  map<int, double> tf_idf_relevance;
  for (const auto [query_id, term_freq] : query_to_doc_relevance) {
    for (const auto & [int_0, int_1] : term_freq) {
      tf_idf_relevance[query_id] += int_1;
    }
  }
 
  for (int i = 0; i < test_documents_tf_idf.size(); ++i) {
    server.AddDocument(test_id[i], test_documents_tf_idf[i], test_status[i], test_rating[i]);
  }

  auto search_results = server.FindTopDocuments("пушистый ухоженный кот"s);
  for (size_t i = 0; i < tf_idf_relevance.size(); ++i) {
        auto search_results = server.FindTopDocuments(queries);
        for (const auto & doc : search_results) {
          const double expected_relevance = tf_idf_relevance.at(doc.id);
          ASSERT_EQUAL(doc.relevance, expected_relevance);
        }
  }
}

void TestSearchServer() {

    RUN_TEST( TestAddDocument                    );
    RUN_TEST( TestMatchDocumentStopWords         );
    RUN_TEST( TestMatchDocumentPlusWords         );
    RUN_TEST( TestMatchDocumentMinusWords        );
    RUN_TEST( TestFindTopDocumentsPlusWords      );
    RUN_TEST( TestFindTopDocumentsMinusWords     );
    RUN_TEST( TestFindTopDocumentsStopWords      );
    RUN_TEST( TestCalculationAverageRating       );
    RUN_TEST( TestFindTopDocumentsbyStatus       );
    RUN_TEST( TestFindTopDocumentsSort           );
    RUN_TEST( TestTfIdfCalculation               );
    RUN_TEST( TestFindTopDocumentsUsingPredicate );

}

int main() {

  TestSearchServer();
  cout << "Search server testing finished"s << endl;

  SearchServer search_server("и в на"s);

  AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
  AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
  AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
  AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

  FindTopDocuments(search_server, "пушистый -пёс"s);
  FindTopDocuments(search_server, "пушистый --кот"s);
  FindTopDocuments(search_server, "пушистый -"s);

  MatchDocuments(search_server, "пушистый пёс"s);
  MatchDocuments(search_server, "модный -кот"s);
  MatchDocuments(search_server, "модный --пёс"s);
  MatchDocuments(search_server, "пушистый - хвост"s);

} 
