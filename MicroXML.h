/*
	This file is part of the E_XML Library
	Copyright (C) 2007-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	Copyright (C) 2018-2019 Sascha Brandt <sascha@brandt.graphics>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef MICROXML_H
#define MICROXML_H

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>

/***
 ** MicroXML
 ** @description Very simple XML-Parser
 **/
namespace E_XML {

typedef std::unordered_map<std::string, std::string> attributes_t;

/**
 * Type of function that is called when a tag is entered.
 * 
 * @param tagName The name of the tag that is entered.
 * @param attributes A map of the tag's attributes.
 * @return If @c true, the traversal should continue. If @c false, the
 * traversal should stop.
 */
typedef std::function<bool (const std::string &, const attributes_t &)> visitor_enter_t;

/**
 * Type of function that is called when a tag is left.
 * 
 * @param tagName The name of the tag that is left.
 * @return If @c true, the traversal should continue. If @c false, the
 * traversal should stop.
 */
typedef std::function<bool (const std::string &)> visitor_leave_t;

/**
 * Type of function that is called when data is read.
 * 
 * @param tagName The name of the tag that contains the data.
 * @param data The data that has been read.
 * @return If @c true, the traversal should continue. If @c false, the
 * traversal should stop.
 */
typedef std::function<bool (const std::string &, const std::string &)> visitor_data_t;

// --------------------

void traverse(std::istream & in,
			  const visitor_enter_t & enterFun,
			  const visitor_leave_t & leaveFun,
			  const visitor_data_t & dataFun);

// --------------------

std::string replace(const std::string& input, const std::string& regex, const std::string& repl, bool extended=false);
int32_t search(const std::string& input, const std::string& regex, std::vector<std::string>& matches, bool extended=false);

}
#endif // MICROXML_H
