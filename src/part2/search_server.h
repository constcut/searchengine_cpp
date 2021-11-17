#pragma once

#include "search_server.h"
#include "synchronized.h"

#include <istream>
#include <ostream>
#include <vector>
#include <string>
#include <string_view>
#include <queue>
#include <future>
#include <map>
using namespace std;

class InvertedIndex
{
public:
    struct Entry
    {
        size_t docid, hitcount;
    };

    InvertedIndex() = default;
    explicit InvertedIndex(istream &documentInput);

    const vector<Entry> &Lookup(string_view word) const;

    const deque<string> &GetDocuments() const
    {
        return docs;
    }

private:
    deque<string> docs;
    map<string_view, vector<Entry>> index;
};

class SearchServer
{
public:
    SearchServer() = default;
    explicit SearchServer(istream & documentInput)
        : index(InvertedIndex(documentInput))
    {
    }

    void UpdateDocumentBase(istream & documentInput);
    void AddQueriesStream(istream & queryInput, ostream & searchResultsOutput);

private:
    Synchronized<InvertedIndex> index;
    vector<future<void>> async_tasks;

    void ProcessRequests(istream &queryInput, ostream &searchResultsOutput,
                         Synchronized<InvertedIndex> &indexHandle);

    void UpdateIndex(istream &documentInput, Synchronized<InvertedIndex> &index);
};
