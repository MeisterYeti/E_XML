/*
	This file is part of the E_XML Library
	Copyright (C) 2009-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2009-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2009-2012 Ralf Petring <ralf@petring.net>
	Copyright (C) 2018-2019 Sascha Brandt <sascha@brandt.graphics>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "E_MicroXMLReader.h"
#include "MicroXML.h"
#include <EScript/Basics.h>
#include <EScript/StdObjects.h>
#include <EScript/Utils/IO/IO.h>
#include <functional>
#include <fstream>
#include <iostream>

using namespace EScript;

namespace E_XML {

static ExtObject * visitorCreateETag(const std::string & tagName) {
	auto o = new ExtObject;

	static const EScript::StringId nameId("name");
	o->setAttribute(nameId, String::create(tagName));

	auto m = new EScript::Map;

	static const EScript::StringId attributesId("attributes");
	o->setAttribute(attributesId, m);
	return o;
}

static ExtObject * visitorCreateETag(const std::string & tagName, const attributes_t & attributes) {
	auto o = new ExtObject;

	static const EScript::StringId nameId("name");
	o->setAttribute(nameId, String::create(tagName));

	auto m = new EScript::Map;

	static const EScript::StringId attributesId("attributes");
	o->setAttribute(attributesId, m);
	for(const auto & attrib : attributes) {
		m->setValue(String::create(attrib.first), String::create(attrib.second));
	}
	return o;
}

static bool visitorEnter(EScript::Runtime & rt, E_MicroXMLReader & reader, const std::string & tagName, const attributes_t & attributes) {
	static const EScript::StringId attrName("enter");
	ObjRef resultObj = callMemberFunction(rt, &reader, attrName, ParameterValues(visitorCreateETag(tagName, attributes)));
	return resultObj.toBool();
}

static bool visitorLeave(EScript::Runtime & rt, E_MicroXMLReader & reader, const std::string & tagName) {
	static const EScript::StringId attrName("leave");
	ObjRef resultObj = callMemberFunction(rt, &reader, attrName, ParameterValues(String::create((tagName))));
	return resultObj.toBool();
}

static bool visitorData(EScript::Runtime & rt, E_MicroXMLReader & reader, const std::string & tagName, const std::string & data) {
	static const EScript::StringId attrName("data");
	ObjRef resultObj = callMemberFunction(rt, &reader, attrName, ParameterValues(String::create(tagName), String::create(data)));
	return resultObj.toBool();
}

static bool visitorParse(EScript::Runtime & rt, E_MicroXMLReader & reader, const std::string & filename) {

	std::ifstream in(filename.c_str(), std::ios_base::in | std::ios_base::binary);
	if(!in.good()) {
		return false;
	}

	using namespace std::placeholders;
	traverse(in,
					 std::bind(visitorEnter, std::ref(rt), std::ref(reader), _1, _2),
					 std::bind(visitorLeave, std::ref(rt), std::ref(reader), _1),
					 std::bind(visitorData, std::ref(rt), std::ref(reader), _1, _2));

	return true;
}

//! (static)
EScript::Type * E_MicroXMLReader::getTypeObject() {
	// E_MicroXMLReader ---|> ExtObject
	static EScript::ERef<EScript::Type> typeObject = new EScript::Type(EScript::ExtObject::getTypeObject());
	return typeObject.get();
}

E_MicroXMLReader::E_MicroXMLReader(EScript::Type * type) :
	ExtObject(type?type:getTypeObject()){
}

E_MicroXMLReader::~E_MicroXMLReader() = default;

//! [E_MicroXMLReader] initMembers
void init(EScript::Namespace* lib) {
	//std::cout << "Loading library XML..." << std::endl;
	if(lib->getAttribute("XML").isNotNull()) {
		//std::cerr << "XML Library already loaded!" << std::endl;
		return;
	}
	auto * ns = new EScript::Namespace;
	declareConstant(lib, "XML", ns);
	
	EScript::Type * typeObject = E_MicroXMLReader::getTypeObject();
	declareConstant(ns,E_MicroXMLReader::getClassName(),typeObject);

	//! [ESF] MicroXMLReader new XML.MicroXMLReader()
	ES_CTOR(typeObject,0,0,new E_MicroXMLReader())

	//! [ESMF] bool XML.MicroXMLReader.data(tagname,data)
	ES_FUN(typeObject,"data",2,2,true)

	//! [ESMF] bool XML.MicroXMLReader.enter(tag)
	ES_FUN(typeObject,"enter",1,1,true)

	//! [ESMF] bool XML.MicroXMLReader.leave(tagname)
	ES_FUN(typeObject,"leave",1,1,true)

	//! [ESMF] bool  XML.MicroXMLReader.parse(Filename)
	ES_MFUN(typeObject,E_MicroXMLReader,"parse",1,1,
			visitorParse(rt, *thisObj, parameter[0].toString()))

	//! [ESF] String XML.replace(String in, String regex, String repl[, Bool extended])
	ES_FUN(ns,"replace",3,4,
			replace(parameter[0].toString(), parameter[1].toString(), parameter[2].toString(), parameter[3].toBool(false)))
			
	//! [ESF] Match XML.search(String in, String regex[, Bool extended])
	ES_FUNCTION(ns,"search",2,3, {
		std::vector<std::string> matches;
		int32_t pos = search(parameter[0].toString(), parameter[1].toString(), matches, parameter[2].toBool(false));
		if(matches.empty())
			return Bool::create(false);
		auto* match = new EScript::ExtObject;
		match->setAttribute("matches", Array::create(matches));
		match->setAttribute("start", Number::create(pos));
		return match;
	})
}

}
