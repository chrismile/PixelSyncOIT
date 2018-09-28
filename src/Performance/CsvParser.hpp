//
// Created by christoph on 27.09.18.
//

#ifndef PIXELSYNCOIT_CSVPARSER_HPP
#define PIXELSYNCOIT_CSVPARSER_HPP

#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <iostream>

typedef std::list<std::vector<std::string>> RowMap;

/*! Parser for the middle_states.csv file
 * \param filename: The filename of the CSV file
 * \param filterComments: Filers lines starting with a hashtag (#)
 * \return A list of rows stored in the CSV file
 */
RowMap parseCSV(const std::string &filename, bool filterComments = true);

#endif //PIXELSYNCOIT_CSVPARSER_HPP
