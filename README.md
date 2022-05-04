## cpp-search-server

Это приложение поисковой системы может найти определенный документ среди всех добавленных документов. Порядок результатов основан на системе рангов приоритета TF-IDF.

Поисковый движок с поддержкой плюс, минус и стоп-слов. Реализована разбивка на страницы и удаление дубликатов документов. Реализован поиск документов, используя фильтры.

Реализован с использованием параллельной версии map, многопоточности, итераторов и исключений.

Поисковая система содержит встроенную защиту от ввода недопустимых символов [(сомволов с кодами от 0 до 31)](http://gymnaz1-murm.ru/wp-content/uploads/2017/10/%D0%A2%D0%B0%D0%B1%D0%BB%D0%B8%D1%86%D0%B0-%D0%BA%D0%BE%D0%B4%D0%BE%D0%B2-ASCII.pdf).

Есть возможность использования последовательной и параллельной версии поиска документов.

Класс поискового сервера инициализируется стоп-словами. Система поддерживает различные типы документов:
* актуальные(ACTUAL),
* удаленные(REMOVED),
* неактуальные(IRRELEVANT),
* запрещенные(BANNED).

# Основные функции

В программе реализовано несколько основных функций для работы с документами:

* добавление документов

```cpp
search_server.AddDocument(<id>, <содержимое>, <статус>)
```

* добавление стоп - слов

```cpp 
SearchServer search_server(<стоп-слова>); // обьявление стоп - слов при создании поисковой системы
search_server.SetStopWords(<стоп-слова>); // добавление дополнительных стоп - слов
```

* поиск по плюс/минус - словам (перед минус словом ставится символ "-" без пробела)

```cpp 
search_server.FindTopDocuments("<плюс-слова> -<минус-слова>"s));
```

* вывод списка документов

```cpp 
PrintDocument(<документ>);
```

* поиск документов, используя фильтры. Фильтрация может производиться по статусу документа, его id и рейтингу.

```cpp 
search_server.FindTopDocuments("<плюс/минус-слова>"s, <фильтр функция>);
```

* удаление дубликатов

```cpp 
RemoveDuplicates(search_server);
```

* изменение версий поискового метода (однопоточная / многопоточная). По умолчанию выбирается однопоточная версия поискового метода.

```cpp 
// см. пример
```

# Пример использования

* Пример: 

```cpp 
#include "process_queries.h"
#include "search_server.h"

#include <execution>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }


    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments(
            execution::seq,
            "curly nasty cat"s,
            DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments(
            execution::par,
            "curly nasty cat"s,
            [](int document_id, DocumentStatus status, int rating) {
                return document_id % 2 == 0;
            })) {
        PrintDocument(document);
    }

    return 0;
} 
```

* Вывод:

```cpp 
ACTUAL by default:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
Even ids:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
```
# Системные требования

1. C++17 (STL)
2. GCC (MinGW-w64) 11.2.0

# Планы по доработке

Добавить возможность чтение/вывод документов из JSON или используя Protobuf.
Реализовать графическое приложение, используя Qt.


