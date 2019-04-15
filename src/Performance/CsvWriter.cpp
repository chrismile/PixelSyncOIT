//
// Created by christoph on 27.09.18.
//

#include <boost/algorithm/string/replace.hpp>
#include <Utils/File/Logfile.hpp>

#include "CsvWriter.hpp"

CsvWriter::CsvWriter()
{
}

CsvWriter::CsvWriter(const std::string &filename)
{
    open(filename);
}

CsvWriter::~CsvWriter()
{
    close();
}


bool CsvWriter::open(const std::string &filename)
{
    file.open(filename.c_str());

    if (!file.is_open()) {
        sgl::Logfile::get()->write(std::string() + "Error in CsvWriter::open: Couldn't open file called \""
                + filename + "\".");
        return false;
    }

    return true;
}

void CsvWriter::close()
{
    if (isOpen) {
        file.close();
        isOpen = false;
    }
}


void CsvWriter::writeRow(const std::vector<std::string> &row)
{
    size_t rowSize = row.size();
    for (size_t i = 0; i < rowSize; i++) {
        file << escapeString(row.at(i));
        if (i != rowSize-1) {
            file << ",";
        }
    }
    file << "\n"; // End of row/line
}


void CsvWriter::writeCell(const std::string &cell)
{
    if (writingRow) {
        file << ",";
    }
    file << escapeString(cell);
    writingRow = true;
}

void CsvWriter::newRow()
{
    writingRow = false;
    file << "\n";
}


std::string CsvWriter::escapeString(const std::string &s)
{
    if (s.find(",") == std::string::npos && s.find("\"") == std::string::npos && s.find("\n") == std::string::npos) {
        // Nothing to escape
        return s;
    }

    // Replace quotes by double-quotes and return string enclosed with single quotes
    return std::string() + "\"" + boost::replace_all_copy(s, "\"", "\"\"") + "\"";
}
