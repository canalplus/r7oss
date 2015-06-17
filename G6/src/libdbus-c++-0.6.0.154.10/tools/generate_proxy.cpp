/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "generator_utils.h"
#include "generate_proxy.h"

using namespace std;
using namespace DBus;

extern const char *tab;
extern const char *header;
extern const char *dbus_includes;

void parse_dbus_introspect_xml(Xml::Node &, const char *, bool, bool, bool,
        ostringstream &, vector<string> &);

void generate_proxy(Xml::Document & doc, const char * filename, const char * suffix,
        bool pass_message, bool with_asyncs, bool with_addmatch_selection)
{
    // open the file where the proxy code will be dumped
    ofstream file(filename);
    if (file.bad())
    {
        cerr << "unable to write file " << filename << endl;
        exit(-1);
    }

    ostringstream head;
    string cond_comp;
    {
        string filestring = filename;
        underscorize(filestring);
        cond_comp = "__dbusxx__" + filestring + "__PROXY_MARSHAL_H";

        head << header;

        head << "#ifndef " << cond_comp << endl
             << "#define " << cond_comp << endl;

        head << dbus_includes;
    }

    vector <string> include_vector;
    ostringstream body;
    // section to unwrap the tp: extensions and call the dbus introspection xml parser
    Xml::Node & root = *(doc.root);
    if (root.name == "tp:spec") {
//        cerr << "tp::spec" << endl;
        Xml::Nodes sections = root["tp:section"];
        for (Xml::Nodes::const_iterator it=sections.begin() ; it!=sections.end() ; ++it) {
//            cerr << tab << (*it)->name << ": \"" << (*it)->get("name") << "\"" << endl;
            Xml::Nodes nodes = (**it)["node"];
            for (Xml::Nodes::const_iterator node_it=nodes.begin() ; node_it!=nodes.end() ; ++node_it) {
//                cerr << tab << tab << (*node_it)->name << ": \"" << (*node_it)->get("name") << "\"" << endl;
                parse_dbus_introspect_xml(**node_it, suffix, pass_message, with_asyncs, with_addmatch_selection, body, include_vector);
                body << endl;
            }
        }
    }
    else {
        parse_dbus_introspect_xml(*(doc.root), suffix, pass_message, with_asyncs, with_addmatch_selection, body, include_vector);
    }

    {
        // remove all duplicates in the header include vector
        vector<string>::const_iterator vec_end_it = unique (include_vector.begin (), include_vector.end ());

        for (vector<string>::const_iterator vec_it = include_vector.begin ();
                vec_it != vec_end_it;
                ++vec_it)
        {
            const string &include = *vec_it;

            head << "#include " << "\"" << include << ".h" << "\"" << endl;
        }
        head << endl;
    }
    file << head.str();
    file << body.str();

    file << "#endif //" << cond_comp << endl;
    file.close();
}

/*! Generate proxy code for a XML introspection
  */
void parse_dbus_introspect_xml(Xml::Node & root, const char *suffix, bool pass_message, bool with_asyncs, bool with_addmatch_selection,
        ostringstream & body, vector<string> & include_vector)
{
	Xml::Nodes interfaces = root["interface"];

	// iterate over all interface definitions
	for (Xml::Nodes::iterator i = interfaces.begin(); i != interfaces.end(); ++i)
	{
		Xml::Node &iface = **i;
		Xml::Nodes methods = iface["method"];
		Xml::Nodes signals = iface["signal"];
		Xml::Nodes properties = iface["property"];
		Xml::Nodes ms;
		ms.insert(ms.end(), methods.begin(), methods.end());
		ms.insert(ms.end(), signals.begin(), signals.end());

		// gets the name of a interface: <interface name="XYZ">
		string ifacename = iface.get("name");

		// these interface names are skipped.
		if (ifacename == "org.freedesktop.DBus.Introspectable"
		        ||ifacename == "org.freedesktop.DBus.Properties"
		        ||ifacename == "org.freedesktop.DBus.ObjectManager")
		{
			cerr << "skipping interface " << ifacename << endl;
			continue;
		}

		istringstream ss(ifacename);
		string nspace;
		unsigned int nspaces = 0;

		// prints all the namespaces defined with <interface name="X.Y.Z">
		while (ss.str().find('.', ss.tellg()) != string::npos)
		{
			getline(ss, nspace, '.');

			body << "namespace " << nspace << " {" << endl;

			++nspaces;
		}
		body << endl;

		string ifaceclass;

		getline(ss, ifaceclass);

                ifaceclass += suffix;

		cerr << "generating code for interface " << ifacename << "..." << endl;

		// the code from class definiton up to opening of the constructor is generated...
		body << "class " << ifaceclass << endl
		<< ": public ::DBus::InterfaceProxy" << endl
		<< "{" << endl
		<< "public:" << endl
		<< endl;
        if (with_addmatch_selection) {
    		if (0<signals.size()) {
	    	    body << tab << ifaceclass << "(bool enable_signals=true)" << endl;
		    }
    		else {
	    	    body << tab << ifaceclass << "(/*bool enable_signals=true*/)" << endl;
		    }
        }
        else {
            body << tab << ifaceclass << "()" << endl;
        }
		body << tab << ": ::DBus::InterfaceProxy(\"" << ifacename << "\")" << endl
		<< tab << "{" << endl;

		stringstream enable_allsignals;
		enable_allsignals << tab << tab << "disable_legacy_mode();" << endl;
		enable_allsignals << tab << tab << "if (enable_signals) {" << endl;
		// generates code to connect all the signal stubs; this is still inside the constructor
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;

			string marshname = "_" + signal.get("name") + "_stub";

			body << tab << tab << "connect_signal("
			<< ifaceclass << ", " << signal.get("name") << ", " << stub_name(signal.get("name"))
			<< ");" << endl;

			enable_allsignals << tab << tab << tab << "pre_enabled_signals_add(\"" << signal.get("name") << "\");" << endl;
		}
		enable_allsignals << tab << tab << "}";
        if (with_addmatch_selection) {
    		if (0 < signals.size()) {
	    	    body << endl << enable_allsignals.rdbuf() << endl;
    		}
        }


        if (with_asyncs) {
		    body << endl;
		    // generates code to connect all method async replies stubs
		    for (Xml::Nodes::iterator mi = methods.begin() ; mi != methods.end() ; ++mi){
			    Xml::Node & method = **mi;

			    string marshname = "_" + method.get("name") + "_reply_stub";

			    body << tab << tab << "connect_reply("
				    	<< ifaceclass << ", " << method.get("name") << ", " << marshname << ");" << endl;
		    }
        }

		// the constructor ends here
		body << tab << "}" << endl
		<< endl;

		// write public block header for properties
		body << "public:" << endl << endl
		<< tab << "/* properties exported by this interface */" << endl;

		// this loop generates all properties
		for (Xml::Nodes::iterator pi = properties.begin ();
		        pi != properties.end (); ++pi)
		{
			Xml::Node & property = **pi;
			string prop_name = property.get ("name");
			string property_access = property.get ("access");
			if (property_access == "read" || property_access == "readwrite")
			{
				body << tab << tab << "const " << signature_to_type (property.get("type"))
				<< " " << prop_name << "() {" << endl;
				body << tab << tab << tab << "::DBus::CallMessage call ;\n ";
				body << tab << tab << tab
				<< "call.member(\"Get\"); call.interface(\"org.freedesktop.DBus.Properties\");"
				<< endl;
				body << tab << tab << tab
				<< "::DBus::MessageIter wi = call.writer(); " << endl;
				body << tab << tab << tab
				<< "const std::string interface_name = \"" << ifacename << "\";"
				<< endl;
				body << tab << tab << tab
				<< "const std::string property_name  = \"" << prop_name << "\";"
				<< endl;
				body << tab << tab << tab << "wi << interface_name;" << endl;
				body << tab << tab << tab << "wi << property_name;" << endl;
				body << tab << tab << tab
				<< "::DBus::Message ret = this->invoke_method (call);" << endl;
				// TODO: support invoke_method_NoReply for properties
				body << tab << tab << tab
				<< "::DBus::MessageIter ri = ret.reader ();" << endl;
				body << tab << tab << tab << "::DBus::Variant argout; " << endl;
				body << tab << tab << tab << "ri >> argout;" << endl;
				body << tab << tab << tab << "return argout;" << endl;
				body << tab << tab << "};" << endl;
			}

			if (property_access == "write" || property_access == "readwrite")
			{
				body << tab << tab << "void " << prop_name << "( const "<< signature_to_type (property.get("type")) << " & input" << ") {" << endl;
				body << tab << tab << tab << "::DBus::CallMessage call ;\n ";
				body << tab << tab << tab <<"call.member(\"Set\");  call.interface( \"org.freedesktop.DBus.Properties\");"<< endl;
				body << tab << tab << tab <<"::DBus::MessageIter wi = call.writer(); " << endl;
				body << tab << tab << tab <<"::DBus::Variant value;" << endl;
				body << tab << tab << tab <<"::DBus::MessageIter vi = value.writer ();" << endl;
				body << tab << tab << tab <<"vi << input;" << endl;
				body << tab << tab << tab <<"const std::string interface_name = \"" << ifacename << "\";" << endl;
				body << tab << tab << tab <<"const std::string property_name  = \"" << prop_name << "\";"<< endl;
				body << tab << tab << tab <<"wi << interface_name;" << endl;
				body << tab << tab << tab <<"wi << property_name;" << endl;
				body << tab << tab << tab <<"wi << value;" << endl;
				body << tab << tab << tab <<"::DBus::Message ret = this->invoke_method (call);" << endl;
				// TODO: support invoke_method_noreply for properties
				body << tab << tab << "};" << endl;
			}
		}

		// write public block header for methods
		body << "public:" << endl
		<< endl
		<< tab << "/* methods exported by this interface," << endl
		<< tab << " * this functions will invoke the corresponding methods on the remote objects" << endl
		<< tab << " */" << endl;

		// this loop generates all methods
		for (Xml::Nodes::iterator mi = methods.begin(); mi != methods.end(); ++mi)
		{
			stringstream async_body;
			stringstream async_reply;
			stringstream async_stub;

			Xml::Node &method = **mi;
			Xml::Nodes args = method["arg"];
			Xml::Nodes args_in = args.select("direction","in");
			Xml::Nodes args_out = args.select("direction","out");
			Xml::Nodes annotations = args["annotation"];
			Xml::Nodes method_annotations = method["annotation"];
			Xml::Nodes annotations_noreply = method_annotations.select("name","org.freedesktop.DBus.Method.NoReply");
			Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
			string arg_object;
			bool annotation_noreply_value = false;

            if (with_asyncs) {
			    body << endl << tab << "/* " << method.get("name") << " */" << endl;
            }

			// parse method level noreply annotations
			if (annotations_noreply.size() > 0)
			{
				string annotation_noreply_value_str = annotations_noreply.front()->get("value");

				if (annotation_noreply_value_str == "true")
				{
					annotation_noreply_value = true;
				}
			}

			if (annotations_object.size() > 0)
			{
				arg_object = annotations_object.front()->get("value");
			}

			async_reply << tab << "virtual void " << method.get("name") << "_reply(";
			if (args_out.size() == 0 || args_out.size() > 1)
			{
				body << tab << "void ";
			}
			else if (args_out.size() == 1)
			{
				if (arg_object.length())
				{
					body << tab << arg_object << " ";
					async_reply << arg_object << " &, ";
				}
				else
				{
					body << tab << signature_to_type(args_out.front()->get("type")) << " ";
					async_reply << signature_to_type(args_out.front()->get("type")) << " &, ";
				}
			}

			body << method.get("name") << "(";
			async_body << tab << "bool " << method.get("name") << "(";

			// generate all 'in' arguments for a method signature
			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				Xml::Nodes annotations = arg["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				// generate basic signature only if no object name available...
				if (!arg_object.length())
				{
					body << "const " << signature_to_type(arg.get("type")) << "& ";
					async_body << "const " << signature_to_type(arg.get("type")) << " & ";
				}
				// ...or generate object style if available
				else
				{
					body << "const " << arg_object << "& ";
					async_body << "const " << arg_object << " & ";

					// store a object name to later generate header includes
					include_vector.push_back (arg_object);
				}

				string arg_name = arg.get("name");
				if (arg_name.length()) {
					body << arg_name;
					async_body << arg_name;
				} else {
					body << "argin" << i;
					async_body << "argin" << i;
				}

				if ((i+1 != args_in.size() || args_out.size() > 1))
					body << ", ";

			    async_body << ", ";
			}
			async_body << "void * __user_data__, bool __on_thread_pool__)" << endl;

			if (args_out.size() > 1)
			{
				// generate all 'out' arguments for a method signature
				unsigned int j = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++j)
				{
					Xml::Node &arg = **ao;
					Xml::Nodes annotations = arg["annotation"];
					Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
					string arg_object;

					if (annotations_object.size() > 0)
					{
						arg_object = annotations_object.front()->get("value");
					}

					// generate basic signature only if no object name available...
					if (!arg_object.length())
					{
						body << signature_to_type(arg.get("type")) << "&";
						async_reply << signature_to_type(arg.get("type")) << " &";
					}
					// ...or generate object style if available
					else
					{
						body << arg_object << "& ";
						async_reply << arg_object << " &";

						// store a object name to later generate header includes
						include_vector.push_back (arg_object);
					}

					string arg_name = arg.get("name");
					if (arg_name.length())
						body << " " << arg_name;
					else
						body << " argout" << j;

					if (j+1 != args_out.size()) {
						body << ", ";
					}
					async_reply << ", ";
				}
			}
			body << ")" << endl;
			async_reply << "void *) {}" << endl;

			body << tab << "{" << endl
			<< tab << tab << "::DBus::CallMessage call;" << endl;
			async_body << tab << "{" << endl
			           << tab << tab << "::DBus::CallMessage call;" << endl;

			if (args_in.size() > 0)
			{
				body << tab << tab << "::DBus::MessageIter wi = call.writer();" << endl
				<< endl;
				async_body << tab << tab << "::DBus::MessageIter wi = call.writer();" << endl
						   << endl;
			}

			// generate all 'in' arguments for a method body
			i = 0;
			for (Xml::Nodes::iterator ai = args_in.begin(); ai != args_in.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				string arg_name = arg.get("name");
				Xml::Nodes annotations = arg["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				if (!arg_name.length())
				{
					arg_name = "argin";
					arg_name += toString <uint> (i);
				}

				// generate extra code to wrap object
				if (arg_object.length())
				{
					body << tab << tab << signature_to_type(arg.get("type")) << "_" << arg_name << ";" << endl;
					body << tab << tab << "_" << arg_name << " << " << arg_name << ";" << endl;
					async_body << tab << tab << signature_to_type(arg.get("type")) << "_" << arg_name << ";" << endl
							   << tab << tab << "_" << arg_name << " << " << arg_name << ";" << endl;

					arg_name = string ("_") + arg_name;
				}

				body << tab << tab << "wi << " << arg_name << ";" << endl;
				async_body << tab << tab << "wi << " << arg_name << ";" << endl;
			}

			body << tab << tab << "call.member(\"" << method.get("name") << "\");" << endl;
			async_body << tab << tab << "call.member(\"" << method.get("name") << "\");" << endl;

			// generate noreply/reply method calls
			if (annotation_noreply_value)
			{
				if (args_out.size ())
				{
					cerr << "Function: " << method.get("name") << ":" << endl;
					cerr << "Option 'org.freedesktop.DBus.Method.NoReply' not allowed for methods with 'out' variables!" << endl << "-> Option ignored!" << endl;

					body << tab << tab << "::DBus::Message ret = invoke_method (call);" << endl;
				}
				else
				{
					body << tab << tab << "assert (invoke_method_noreply (call));" << endl; // will only assert in case of no memory
				}
			}
			else
			{
				body << tab << tab << "::DBus::Message ret = invoke_method (call);" << endl;
			}

			async_body << tab << tab << "return invoke_method_async(call, __user_data__, __on_thread_pool__);" << endl
			           << tab << "}" << endl << endl;

            if (0==args_out.size()) {
			    async_stub << tab << "void _" << method.get("name") << "_reply_stub(const ::DBus::ReturnMessage & /*rmsg*/, void * udata)" << endl;
            }
            else {
			    async_stub << tab << "void _" << method.get("name") << "_reply_stub(const ::DBus::ReturnMessage & rmsg, void * udata)" << endl;
            }
            async_stub << tab << "{"
					   << endl;

			if (args_out.size() > 0)
			{
				body << tab << tab << "::DBus::MessageIter ri = ret.reader();" << endl
				<< endl;
				async_stub << tab << tab << "::DBus::MessageIter ri = rmsg.reader();" << endl
						   << endl;
			}

			// generate 'out' values as return if only one existing
			stringstream async_reply_args;
			if (args_out.size() == 1)
			{
				Xml::Nodes annotations = args_out["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				if (arg_object.length())
				{
					body << tab << tab << arg_object << " _argout;" << endl;
					async_stub << tab << tab << arg_object << " _argout" << endl;
				}

				body << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout;" << endl;

				body << tab << tab << "ri >> argout;" << endl;

				async_stub << tab << tab << signature_to_type(args_out.front()->get("type")) << " argout;" << endl;
				async_stub << tab << tab << "ri >> argout;" << endl;

				if (arg_object.length())
				{
					body << tab << tab <<  "_argout << argout;" << endl;
					body << tab << tab << "return _argout;" << endl;

					async_stub << tab << tab << "_argout << argout;" << endl;
					async_reply_args << "_argout, ";
				}
				else
				{
					body << tab << tab << "return argout;" << endl;
					async_reply_args << "argout, ";
				}
			}
			else if (args_out.size() > 1)
			{
				// generate multible 'out' value
				unsigned int i = 0;
				for (Xml::Nodes::iterator ao = args_out.begin(); ao != args_out.end(); ++ao, ++i)
				{
					Xml::Node &arg = **ao;
					string arg_name = arg.get("name");
					Xml::Nodes annotations = arg["annotation"];
					Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
					string arg_object;

					if (annotations_object.size() > 0)
					{
						arg_object = annotations_object.front()->get("value");
					}

					if (!arg_name.length())
					{
						arg_name = "argout" + toString <uint> (i);
					}

					if (arg_object.length())
					{
						body << tab << tab << signature_to_type(arg.get("type")) << " _" << arg_name << ";" << endl;
						body << tab << tab << "ri >> " << "_" << arg_name << ";" << endl;
						body << tab << tab << arg_name << " << " << "_" << arg_name << ";" << endl;
					}
					else
					{
						body << tab << tab << "ri >> " << arg_name << ";" << endl;
					}

					async_stub << tab << tab << signature_to_type(arg.get("type")) << " " << arg_name << ";" << endl;
					async_stub << tab << tab << "ri >> " << arg_name << ";" << endl;

					async_reply_args << arg_name << ", ";
				}
			}

			body << tab << "}" << endl
			<< endl;

            async_reply_args << "udata";
			async_stub << tab << tab << method.get("name") << "_reply(" << async_reply_args.rdbuf() << ");" << endl;
			async_stub << tab << "}" << endl
					   << endl;

            if (with_asyncs) {
			    body << async_body.rdbuf();
			    body << async_reply.rdbuf();
			    body << async_stub.rdbuf();
            }
		}

		// write public block header for signals
		body << endl
		<< "public:" << endl
		<< endl
		<< tab << "/* signal handlers for this interface" << endl
		<< tab << " */" << endl;

		// this loop generates all signals
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];
			bool prepend_param_sep;

			body << tab << "virtual void " << signal.get("name") << "(";

			if (pass_message)
			{
				body << "const ::DBus::SignalMessage & _msg_";
				prepend_param_sep = true;
			}
			else
			{
				prepend_param_sep = false;
			}

			// this loop generates all argument for a signal
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai)
			{
				Xml::Node &arg = **ai;
				Xml::Nodes annotations = arg["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (prepend_param_sep) {
					body << ", ";
					prepend_param_sep = false;
				}

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				// generate basic signature only if no object name available...
				if (!arg_object.length())
				{
					body << "const " << signature_to_type(arg.get("type")) << "& ";
				}
				// ...or generate object style if available
				else
				{
					body << "const " << arg_object << "& ";

					// store a object name to later generate header includes
					include_vector.push_back (arg_object);
				}

				if ((ai+1 != args.end()))
					body << ", ";
			}
			body << ") {};" << endl;
		}

		// write private block header for unmarshalers
		body << endl
		<< "private:" << endl
		<< endl
		<< tab << "/* unmarshalers (to unpack the DBus message before calling the actual signal handler)" << endl
		<< tab << " */" << endl;

		// generate all the unmarshalers
		for (Xml::Nodes::iterator si = signals.begin(); si != signals.end(); ++si)
		{
			Xml::Node &signal = **si;
			Xml::Nodes args = signal["arg"];

			body << tab << "void " << stub_name(signal.get("name"));

			if (args.size() > 0)
			{
				body << "(const ::DBus::SignalMessage &sig)" << endl
				<< tab << "{" << endl
				<< tab << "::DBus::MessageIter ri = sig.reader();" << endl
				<< endl;
			}
			else
			{
				body << "(const ::DBus::SignalMessage &";
 				if (pass_message)
				{
					body << "sig";
				}
				body << ")" << endl
				<< tab << "{" << endl;
			}

			unsigned int i = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++i)
			{
				Xml::Node &arg = **ai;
				string arg_name = arg.get("name");
				Xml::Nodes annotations = arg["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				body << tab << tab << signature_to_type(arg.get("type")) << " " ;

				// use a default if no arg name given
				if (!arg_name.length())
				{
					arg_name = "arg" + toString <uint> (i);
				}

				body << arg_name << ";" << endl;
				body << tab << tab << "ri >> " << arg_name << ";" << endl;

				// if a object type is used create a local variable and insert values with '<<' operation
				if (arg_object.length())
				{
					body << tab << tab << arg_object << " _" << arg_name << ";" << endl;
					body << tab << tab << "_" << arg_name << " << " << arg_name << ";" << endl;

					// store a object name to later generate header includes
					include_vector.push_back (arg_object);
				}
			}

			body << tab << tab << signal.get("name") << "(";

			bool prepend_param_sep;

			if (pass_message)
			{
				body << "sig";
				prepend_param_sep = true;
			}
			else
			{
				prepend_param_sep = false;
			}

			// generate all arguments for the call to the virtual function
			unsigned int j = 0;
			for (Xml::Nodes::iterator ai = args.begin(); ai != args.end(); ++ai, ++j)
			{
				Xml::Node &arg = **ai;
				string arg_name = arg.get("name");
				Xml::Nodes annotations = arg["annotation"];
				Xml::Nodes annotations_object = annotations.select("name","org.freedesktop.DBus.Object");
				string arg_object;

				if (prepend_param_sep) {
					body << ", ";
					prepend_param_sep = false;
				}

				if (annotations_object.size() > 0)
				{
					arg_object = annotations_object.front()->get("value");
				}

				if (!arg_name.length())
				{
					arg_name = "arg" + toString <uint> (j);
				}

				if (arg_object.length())
				{
					body << "_" << arg_name;
				}
				else
				{
					body << arg_name;
				}

				if (ai+1 != args.end())
					body << ", ";
			}

			body << ");" << endl;

			body << tab << "}" << endl;
		}

        //write protected enabler/disablers for signals
        if (with_addmatch_selection) {
            body << endl
            << "protected:" << endl
            << endl
            << tab << "/* signal enablers/disablers for this interface" << endl
            << tab << " */" << endl << endl;

            for (Xml::Nodes::iterator si = signals.begin() ; si != signals.end() ; ++si) {
                Xml::Node & signal =  **si;

                body << tab << "/* " << signal.get("name") << " */" << endl;

                body << tab << "void enable_signal_" << signal.get("name") << "()" << endl;
                body << tab << "{" << endl;
                body << tab << tab << "enable_signal(\"" << signal.get("name") << "\");" << endl;
                body << tab << "}" << endl;
                body << tab << "void disable_signal_" << signal.get("name") << "()" << endl;
                body << tab << "{" << endl;
                body << tab << tab << "disable_signal(\"" << signal.get("name") << "\");" << endl;
                body << tab << "}" << endl;
            }
        }

		body << "};" << endl
		<< endl;

		for (unsigned int i = 0; i < nspaces; ++i)
		{
			body << "} ";
		}
		body << endl;
	}
}
