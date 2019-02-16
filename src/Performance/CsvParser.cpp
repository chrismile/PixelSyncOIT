//
// Created by christoph on 27.09.18.
//

#include "CsvParser.hpp"

RowMap parseCSV(const std::string &filename, bool filterComments) {
    RowMap rows;

    // Open the file and load its content
    std::ifstream file(filename, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "ERROR in parseCSV: File " << filename << "doesn't exist!" << std::endl;
        exit(1);
    }
    std::string fileContent = std::string((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
    file.close();
    size_t fileLength = fileContent.size();

    std::vector<std::string> row;
    std::string currCell;
    bool stringMode = false;
    for (size_t i = 0; i < fileLength; ++i) {
        // Load data from string
        char cCurr = fileContent.at(i);
        char cNext = '\0';
        if (i < fileLength-1) {
            cNext = fileContent.at(i+1);
        }

        if (filterComments && cCurr == '#') {
            while (fileContent.at(i) != '\n') {
                ++i;
            }
            continue;
        }

        // Check for (") in string
        if (cCurr == '\"') {
            if (cNext == '\"' && stringMode) {
                currCell += cCurr;
                ++i;
            } else {
                stringMode = !stringMode;
            }
            continue;
        }

        // String mode: Just add character
        if (stringMode) {
            currCell += cCurr;
        } else {
            // New cell
            if (cCurr == ',') {
                row.push_back(currCell);
                currCell = "";
            }

                // New line/row
            else if (cCurr == '\n') {
                if (currCell.size() != 0) {
                    row.push_back(currCell);
                    currCell = "";
                }
                rows.push_back(row);
                row.clear();
            }

                // Normal character
            else {
                currCell += cCurr;
            }
        }
    }

    // Any data left?
    if (currCell.size() != 0) {
        row.push_back(currCell);
        currCell = "";
    }
    if (row.size() != 0) {
        rows.push_back(row);
        row.clear();
    }

    return rows;
}

// Test main method
/*int main() {
    RowMap rows = parseCSV("test.csv");

    for (auto &row : rows) {
        for (string &cell : row) {
            cout << cell << endl;
        }
        cout << "--- NEWROW ---" << endl;
    }
    cout << "--- END OF FILE ---" << endl;

    return 0;
}*/