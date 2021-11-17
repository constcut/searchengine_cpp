#include "search_server.h"
#include "iterator_range.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <iostream>

#include <numeric>



vector<string> SplitIntoWordsOld(const string& line) {
  istringstream words_input(line);
  return {istream_iterator<string>(words_input), istream_iterator<string>()};
}


vector<string_view> SplitIntoWords(string_view line) {
    vector<string_view> words;

    size_t firstChar = line.find_first_not_of(' ');
    if (firstChar != 0)
        line.remove_prefix(firstChar); //Удалим префиксные пробелы

    size_t lastSpace = line.find_first_of(' ');

    while (lastSpace != string::npos) {

        string_view newWord = line.substr(0, lastSpace);
        words.push_back(newWord);
        line.remove_prefix(lastSpace);

        size_t notSpace = line.find_first_not_of(' ');

        if (notSpace == string::npos)
            break; //Остались одни пробелы - все слова уже в векторе

        line.remove_prefix(notSpace);
        lastSpace = line.find_first_of(' ');

        if (notSpace != string::npos && lastSpace == string::npos)
            words.push_back(line); //Осталось положить только последнее слово, оно без пробелов в конце
    }

    return words;
}


SearchServer::SearchServer(istream& document_input) {
  UpdateDocumentBase(document_input);
}



void SearchServer::UpdateDocumentBase(istream& document_input) {
  InvertedIndex new_index;

  for (string current_document; getline(document_input, current_document);) 
    new_index.Add(move(current_document));

  index = move(new_index);
}



void SearchServer::AddQueriesStream(istream& query_input, ostream& search_results_output) {

    const size_t docsSize = index.GetDocumentsSize(); 

    vector<size_t> docid_count(docsSize); // map<size_t, size_t> docid_count;
    vector<uint32_t> search_results(docsSize); // vector<pair<size_t, size_t>> search_results(docid_count.begin(), docid_count.end());

    for (string current_query; getline(query_input, current_query);) {

        docid_count.assign(docsSize, 0); // Очистка перед новым запросом

        auto words = SplitIntoWordsOld(current_query); 

        for (auto& word : words)
            for (const auto& docHit : index.Lookup(word))  
                docid_count[docHit.first] += docHit.second; //first docid, second hitcount
 
        iota(search_results.begin(), search_results.end(), 0); // Множество индексов всех документов, начиная с 0

        partial_sort(search_results.begin(), // Частичная сортировка, начиная с search_results.begin()
            Head(search_results, 5).end(), // middle - граница определяющая сколько элементов нужно отсортировать
            search_results.end(),          // с middle по end элементы не будут отсортированны
            [&docid_count](int32_t lhs, int32_t rhs) { //lamda для 
                return make_pair(docid_count[lhs], -lhs) > make_pair(docid_count[rhs], -rhs);} ); //Сравниваем по количеству вхождений документа, и индексу
            //Индекс элементов отрицательный, чтобы при одинаковом количестве вхождений, вначале был выбран более низкий(ранний) docid

        search_results_output << current_query << ':';
        for (size_t docid : Head(search_results, 5)) { 

            if (docid_count[docid] == 0) //Если количество вхождений нулевое - игнорируем
                break;

            search_results_output << " {"
            << "docid: " << docid << ", "
            << "hitcount: " << docid_count[docid] << '}'; 
        }
        search_results_output << endl;
    }
}




void InvertedIndex::Add(string document) {
    docs.push_back(move(document));
    const size_t docid = docs.size() - 1;
    for (auto& word : SplitIntoWordsOld(docs.back())) {
        auto& indexForWord = index[word];
        if (indexForWord.empty() || indexForWord.back().first != docid)
            indexForWord.push_back({ docid, 1 }); //docid + hitcount
        else
            ++indexForWord.back().second; //hitcount - посчитаем для каждого docid сразу, вместо единичных записей в лист
    }
}



vector<pair<size_t, size_t>> InvertedIndex::Lookup(string word) const {
    if (auto it = index.find(word); it != index.end()) 
        return it->second;
    return {};
}
