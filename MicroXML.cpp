 /*
 	This file is part of the E_XML Library
	Copyright (C) 2007-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	Copyright (C) 2018 Sascha Brandt <sascha@brandt.graphics>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "MicroXML.h"
#include <EScript/Utils/StringUtils.h>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <stack>

#define FAIL() throw std::runtime_error("Runtime error.");
#define WARN(msg) std::cerr << msg << std::endl;

namespace E_XML {

class BufferedStreamReader{
		static const size_t capacity = 8192;
		std::istream & is;
		char buffer[capacity];
		size_t cursor;
		size_t end;
		
	public:
		BufferedStreamReader(std::istream & _is) : is(_is),cursor(0),end(0){
			if(is.good()){
				is.read(buffer, capacity);
				const auto extracted = is.gcount();
				end = extracted;
			}
		}
		bool good()const	{	return cursor<end;	}

		char get(){
			const char c = buffer[cursor];
			++cursor;
			if(cursor >= end-1){
				if(end==capacity){ // probably not the last chunk
					buffer[0] = buffer[cursor]; // move peek char
					cursor = 0;
					if(is.good()){
						is.read(buffer+1, capacity-1);
						const auto extracted = is.gcount();
						end = extracted+1;
					}else{
						end = 1;
					}
				}//  else eof
			}
			return c;
		}
		BufferedStreamReader & get(std::streambuf & out,char delim){	
			while(good()&&peek()!=delim)
				out.sputc(get());
			return *this;
		}

		char peek()const{
			return buffer[cursor];
		}
//		BufferedStreamReader(std::istream & _is) : is(_is),cursor(0){}
//		bool good()const	{	return is.good();	}
//		char get()			{	return is.get();	}
//		BufferedStreamReader & get(std::streambuf & out,char delim){	
//				is.get(out,delim);
//				return *this;
//		}
//		char peek()const{	return is.peek();	}
};


static bool consume(BufferedStreamReader & is,const std::string & s) {
	for(const auto c : s){
		if( !is.good() || is.get()!=c )
			return false;
	}
	return true;
}
	
static void stepWhitespaces(BufferedStreamReader & is) {
	while (is.good() && isspace(is.peek()))
		is.get();
}

//! (static)
static std::string readQuotedString(BufferedStreamReader & is){
	char c=is.peek();
	if (c!='"' && c!='\'')
		return "";

	std::string s;
	const char stringMarker = c;
	is.get();
	while(true){
		c = is.peek();
		if(c==0 || !is.good())
			break;// TODO: Warn?

		else if(c==stringMarker){
			is.get();
			break;
		}else{
			s+=c;
			is.get();
		}
	}
	return s;
}

static bool getNextAttribute(std::string & key, std::string & value, BufferedStreamReader & in) {
	stepWhitespaces(in);

	char c=in.peek();
	if( c == '/' || c=='>'|| c=='?')
		return false;

	key="";
	value="";
	while(true){
		c=in.peek();
		if( c == 0 || !in.good()) {
			FAIL();
		} else if( c == '/' || c=='>'|| c=='?') {
			return true;
		}else if( c =='=' ){
			in.get();
			break;
		}
		key+=c;
		in.get();
	}
	stepWhitespaces(in);
	value = readQuotedString(in);

	// convert XML-special character
	// \todo some nicer way to do this...
	if(value.find('&') != std::string::npos ){
		static const std::vector<std::pair<std::string,std::string>> findReplace={
			{ "&quot;", "\""},
			{ "&apos;", "'"	},
			{ "&lt;", 	"<"	},
			{ "&gt;", 	">"	},
			{ "&amp;",	"&"	},
			{ "&#10;",	"\n"},
			{ "&#xA;",	"\n"},
			{ "&#9;",	"\t"}
		};
		value = EScript::StringUtils::replaceMultiple(value, findReplace, -1);
	}
	return true;
}

struct Tag {
	std::string name; // or data
	attributes_t attributes;
	enum tagType_t {
		TAG_TYPE_OPENING, TAG_TYPE_CLOSING, TAG_TYPE_EMPTY, TAG_TYPE_META, TAG_TYPE_INVALID, TAG_TYPE_DATA
	} type;

	Tag() : name(), attributes(), type(TAG_TYPE_INVALID) {}
	Tag(tagType_t t, std::string n) : name(n), attributes(), type(t) {}
	Tag(Tag &&) = default;
	~Tag() = default;

	std::string getAttribute(const std::string & aName)const;
	bool isAttributeSet(const std::string & aName)const;
};

//!  \note Comments inside Tags are not supported.
static Tag getNextTag(BufferedStreamReader & in, std::size_t & currentLine) {
	int skipping=0;
	while(true){
		// read stuff ...
		char c=in.get();//data[cursor];
		if(!in.good() || c==0)
			return Tag();
		else if (c=='"'){ // Buggy
			skipping^=1;
			continue;
		}else if (c=='\''){// Buggy
			skipping^=2;
			continue;
		}else if (c=='\n'){
			++currentLine;
			continue;
		}else if (skipping || c!='<'){
			continue;
		}
		// read tag type
		Tag::tagType_t type=Tag::TAG_TYPE_OPENING;
		c = in.peek();
		if(!in.good() || c==0)
			return Tag();
		else if( c == '/' ){
			type=Tag::TAG_TYPE_CLOSING;
			in.get();
		}else if( c == '?' ){
			type=Tag::TAG_TYPE_META;
			in.get();
		}
////////		else if( c == '!' && StringUtils::beginsWith(in,"!--") ){ // skip comment
		else if( c == '!' ){ // skip comment
			in.get(); // consume '!'
			if(in.peek() == '-'){ // comment
				// search for '-->' using a sliding window.
				char charWindow[] = "   ";
				static const std::string COMMENT_ENDING("-->");
				while(true){
					charWindow[0] = charWindow[1];
					charWindow[1] = charWindow[2];
					charWindow[2] = in.get();
					if(!in.good())
						FAIL();
					if(COMMENT_ENDING==charWindow)
						break;
				}
				return getNextTag(in, currentLine);
			}else if(consume( in, "[CDATA[" )){
				std::ostringstream data;
				
				char charWindow[] = "   ";
				static const std::string CDATA_ENDING("]]>");
				while(true){
					data<<charWindow[0];
					charWindow[0] = charWindow[1];
					charWindow[1] = charWindow[2];
					charWindow[2] = in.get();
					if(!in.good())
						FAIL();
					if(CDATA_ENDING==charWindow)
						break;
				}
				return Tag(Tag::TAG_TYPE_DATA,data.str());
			}else{
				std::cerr << "Invalid tag: <!"<<in.peek()<<"...";
				FAIL();
			}
		}

		// read tag name
		std::string name;
		while(true){
			char _c=in.peek();
			if(!in.good() || _c==0)
				FAIL();
			if(isspace(_c) || _c=='/' || _c== '>')
				break;
			name+=_c;
			in.get();
		}
		Tag tag(type,name);

		// read attributes
		std::string key;
		std::string value;
		while(getNextAttribute(key, value, in)){
			tag.attributes[key]=value;
		}
		stepWhitespaces(in);
		c = in.peek();
		if(!in.good() || c==0) {
			FAIL();
		} else if(c=='?' && tag.type==Tag::TAG_TYPE_META){
			in.get();
			stepWhitespaces(in);
		}else if(c=='/'){
			if(tag.type==Tag::TAG_TYPE_OPENING) {
				tag.type=Tag::TAG_TYPE_EMPTY;
			} else {
				FAIL();
			}
			in.get();
			stepWhitespaces(in);
		}
		if(in.peek()!='>') {
			FAIL();
		}

		in.get();
		return tag;
	}
}

static std::string getData(BufferedStreamReader & in) {
	if (in.peek() == '<') {
		return std::string();
	}
	std::stringbuf buffer(std::ios_base::out);

	in.get(buffer, '<');
	if(!in.good()){
		//WARN("X");
	}
	return buffer.str();
}

void traverse(std::istream & in,
					  const visitor_enter_t & enterFun,
					  const visitor_leave_t & leaveFun,
					  const visitor_data_t & dataFun) {
	if(!in.good()) {
		WARN("Invalid stream.");
		return;
	}
	std::size_t currentLine = 0;
	in.seekg(0, std::ios::beg);

	BufferedStreamReader is(in);

	std::stack<Tag> tags;
	bool finished = false;
	while(!finished) {
		Tag tag( getNextTag(is, currentLine) );
		switch(tag.type){
			case Tag::TAG_TYPE_INVALID:
				finished = true;
				break;
			case Tag::TAG_TYPE_OPENING:{
				tags.emplace(std::move(tag));
				if(!enterFun(tags.top().name, tags.top().attributes)) {
					finished = true;
					break;
				}
				const std::string data = EScript::StringUtils::trim(getData(is));
				if(!data.empty() && !dataFun(tags.top().name, data))
					finished = true;
				break;
			}
			case Tag::TAG_TYPE_CLOSING:{
				if(tags.empty() || tag.name != tags.top().name){
					WARN(std::string("XML-Error"));
					finished = true;
					break;
				}
				if(!leaveFun(tag.name)){
					finished = true;
					break;
				}
				tags.pop();
				
				const std::string data = EScript::StringUtils::trim(getData(is));
				if(!data.empty() && !dataFun(tags.top().name, data))
					finished = true;
				break;
			}
			case Tag::TAG_TYPE_EMPTY:{
				if(!enterFun(tag.name, tag.attributes)){
					finished = true;					
					break;
				}
				if(!leaveFun(tag.name))
					finished = true;
				
				const std::string data = EScript::StringUtils::trim(getData(is));
				if(!data.empty() && !dataFun(tags.top().name, data))
					finished = true;
				break;
			}
			case Tag::TAG_TYPE_DATA:{
				if( tags.empty() || !dataFun(tags.top().name, tag.name))
					finished = true;
				break;
			}
			case Tag::TAG_TYPE_META:  //! meta tags are ignored. This occurs normally only once per file for the initial <?xml ...?> tag.
			default:;
		}

	}

}

// -------------------------------------------------------------------------------------------------------------

}

