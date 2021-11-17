#include "search_server.h"

#include <algorithm>
#include <future>
#include <numeric>

#include "parse.h"
#include "iterator_range.h"


InvertedIndex::InvertedIndex(istream& documentInput) {
    for (string currentDocument; getline(documentInput, currentDocument); ) {
        docs.push_back(move(currentDocument));
        size_t docid = docs.size() - 1;
        for (string_view word : SplitIntoWordsView(docs.back())) {
            auto& docids = index[word];
            if (!docids.empty() && docids.back().docid == docid) 
                ++docids.back().hitcount;
            else 
                docids.push_back({docid, 1});
        }
    }
}


const vector<InvertedIndex::Entry>& InvertedIndex::Lookup(string_view word) const {
    static const vector<Entry> empty;
    if (auto it = index.find(word); it != index.end()) 
        return it->second;
    else 
        return empty;
}


void SearchServer::UpdateIndex(istream& documentInput, Synchronized<InvertedIndex>& index) {
    InvertedIndex new_index(documentInput);
    swap(index.GetAccess().value, new_index);
}


void SearchServer::ProcessRequests(istream& queryInput, ostream& searchResultsOutput,
    Synchronized<InvertedIndex>& indexHandle) {

    vector<size_t> docid_count;
    vector<int64_t> docids;

    for (string current_query; getline(queryInput, current_query); ) {
        const auto words = SplitIntoWordsView(current_query);
        {
            auto access = indexHandle.GetAccess();
            const size_t doc_count = access.value.GetDocuments().size();
            docid_count.assign(doc_count, 0);
            docids.resize(doc_count);
            auto& index = access.value;
            for (const auto& word : words) 
            for (const auto& [docid, hit_count] : index.Lookup(word)) 
                docid_count[docid] += hit_count;
        }
        iota(docids.begin(), docids.end(), 0);
        partial_sort(begin(docids), Head(docids, 5).end(), end(docids),
            [&docid_count](int64_t lhs, int64_t rhs) {
                return pair(docid_count[lhs], -lhs) > pair(docid_count[rhs], -rhs);});

        searchResultsOutput << current_query << ':';
        for (size_t docid : Head(docids, 5)) {
            const size_t hit_count = docid_count[docid];
            if (hit_count == 0) 
                break;
            searchResultsOutput << " {docid: " << docid << ", "
                << "hitcount: " << hit_count << '}';
        }
        searchResultsOutput << '\n';
    }
}


void SearchServer::UpdateDocumentBase(istream& documentInput) {
  async_tasks.push_back(async(&SearchServer::UpdateIndex, this, ref(documentInput), ref(index)));
}


void SearchServer::AddQueriesStream(istream& queryInput, 
    ostream& searchResultsOutput) {
    async_tasks.push_back(async(
        &SearchServer::ProcessRequests, this, ref(queryInput), ref(searchResultsOutput), ref(index)));
}