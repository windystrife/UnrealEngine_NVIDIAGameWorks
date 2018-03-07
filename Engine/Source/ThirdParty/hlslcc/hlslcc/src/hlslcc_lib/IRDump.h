// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#ifndef IRDUMP_H
#define IRDUMP_H

extern void IRDump( struct exec_list* ir, struct _mesa_glsl_parse_state* State = 0, const char* Header = "");
extern void IRDump(ir_instruction* ir);
extern void IRDump(ir_instruction* IRFirst, ir_instruction* IRLast);

class DebugPrintVisitor : public ir_visitor
{
public:
	DebugPrintVisitor(bool bSingleEntry = false);
   virtual ~DebugPrintVisitor();

   void Indent();

   static void Dump(exec_list* List, _mesa_glsl_parse_state* State);

   /**
    * \name Visit methods
    *
    * As typical for the visitor pattern, there must be one \c visit method for
    * each concrete subclass of \c ir_instruction.  Virtual base classes within
    * the hierarchy should not have \c visit methods.
    */
   /*@{*/
   virtual void visit(ir_rvalue*);
   virtual void visit(ir_variable*);
   virtual void visit(ir_function_signature*);
   virtual void visit(ir_function*);
   virtual void visit(ir_expression*);
   virtual void visit(ir_texture*);
   virtual void visit(ir_swizzle*);
   virtual void visit(ir_dereference_variable*);
   virtual void visit(ir_dereference_array*);
   virtual void visit(ir_dereference_image*);
   virtual void visit(ir_dereference_record*);
   virtual void visit(ir_assignment*);
   virtual void visit(ir_constant*);
   virtual void visit(ir_call*);
   virtual void visit(ir_return*);
   virtual void visit(ir_discard*);
   virtual void visit(ir_if*);
   virtual void visit(ir_loop*);
   virtual void visit(ir_loop_jump*);
   virtual void visit(ir_atomic*);
   /*@}*/

private:
   void PrintID( ir_instruction* ir );
   void PrintType( const glsl_type* Type );
   std::string GetVarName( ir_variable* var );
   void PrintBlockWithScope( exec_list& ir );
   typedef std::map<ir_variable*, std::string> TNameMap;
   TNameMap NameMap;

   typedef std::set<std::string> TNameSet;
   TNameSet UniqueNames;

   typedef std::set<ir_variable*> TVarSet;
   TVarSet UniqueVars;

   bool bIRVarEOL;
   int Indentation;
   int ID;
   bool bDumpBuiltInFunctions;
};

#endif
