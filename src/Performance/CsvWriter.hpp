//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_CSVWRITER_HPP
#define PIXELSYNCOIT_CSVWRITER_HPP

#include <string>
#include <vector>
#include <fstream>

class CsvWriter {
public:
    CsvWriter();
    CsvWriter(const std::string &filename);
    ~CsvWriter();

    bool open(const std::string &filename);
    void close();

    // Note: All writing functions use "escapeString" to convert strings to a format valid for CSV.
    // For more details see: https://en.wikipedia.org/wiki/Comma-separated_values

    /**
     * Writes a row of string to the CSV file. The row is escaped internally if necessary.
     * NOTE: After a call to "writeCell", "writeRow" can only be called after calling "newRow" to end the row.
     */
    void writeRow(const std::vector<std::string> &row);

    /**
     * Writes a single cell string to the CSV file. The string is escaped internally if necessary.
     */
    void writeCell(const std::string &cell);
    void newRow();

private:
    std::string escapeString(const std::string &s);
    bool isOpen = false;
    bool writingRow = false;
    std::ofstream file;
};


#endif //PIXELSYNCOIT_CSVWRITER_HPP
