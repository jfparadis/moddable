/*
 * Copyright (c) 2016-2017  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Tools.
 * 
 *   The Moddable SDK Tools is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 * 
 *   The Moddable SDK Tools is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with the Moddable SDK Tools.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *
 *       Copyright (C) 2010-2016 Marvell International Ltd.
 *       Copyright (C) 2002-2010 Kinoma, Inc.
 *
 *       Licensed under the Apache License, Version 2.0 (the "License");
 *       you may not use this file except in compliance with the License.
 *       You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *       Unless required by applicable law or agreed to in writing, software
 *       distributed under the License is distributed on an "AS IS" BASIS,
 *       WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *       See the License for the specific language governing permissions and
 *       limitations under the License.
 */

#include "xsl.h"
#if mxWindows
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/utime.h>
	#define c_stat _stat
	#define c_utime _utime
	#define c_utimbuf _utimbuf 
#else
	#include <utime.h>
	#define c_stat stat
	#define c_utime utime
	#define c_utimbuf utimbuf 
#endif

static txBoolean fxFindScript(txMachine* the, txString path, txID* id);
static void fxFreezeBuiltIn(txMachine* the);
static void fxFreezeBuiltIns(txMachine* the);
static txScript* fxLoadScript(txMachine* the, txString path);

int main(int argc, char* argv[]) 
{
	txLinker _linker;
	txLinker* linker = &_linker;
	txLinkerInclude** includeAddress;
	txLinkerPreload** preloadAddress;
	txLinkerResource** resourceAddress;
	txLinkerScript** scriptAddress;
	txLinkerStrip** stripAddress;
	int argi;
	txString base = NULL;
  	txString input = NULL;
  	txString output = NULL;
  	txString separator = NULL;
  	txBoolean archiving = 0;
  	txBoolean optimizing = 0;
#if mxWindows
	txString url = "\\";
#else
	txString url = "/";
#endif
	char name[C_PATH_MAX];
	char path[C_PATH_MAX];
	txSize size;
	txLinkerInclude* include;
	txLinkerPreload* preload;
	txLinkerResource* resource;
	txLinkerScript* script;
	FILE* file = NULL;
	xsCreation _creation = {
		128 * 1024 * 1024, 	/* initialChunkSize */
		16 * 1024 * 1024, 	/* incrementalChunkSize */
		4 * 1024 * 1024, 	/* initialHeapCount */
		1 * 1024 * 1024,	/* incrementalHeapCount */
		1024,				/* stackCount */
		2048+2048,			/* keyCount */
		1993,				/* nameModulo */
		127,				/* symbolModulo */
		32 * 1024,			/* parserBufferSize */
		1993,				/* parserTableModulo */
	};
	xsCreation* creation = &_creation;
	xsMachine* the = NULL;
	txInteger count;
	
	txBoolean incremental = 0;
	struct c_stat headerStat;
	struct c_utimbuf headerTimes;
	
	fxInitializeLinker(linker);
	if (c_setjmp(linker->jmp_buf) == 0) {
		if ((argc == 2) && (c_strcmp(argv[1], "args.txt") == 0)) {
			txString buffer;
			txString string;
			char c;
			file = fopen(argv[1], "r");
			mxThrowElse(file);
			fseek(file, 0, SEEK_END);
			size = ftell(file);
			fseek(file, 0, SEEK_SET);
			buffer = fxNewLinkerChunk(linker, size + 1);
			size = fread(buffer, 1, size, file);
			buffer[size] = 0;
			fclose(file);
			string = buffer;
			argc = 1;
			while ((c = *string++)) {
				if (isspace(c))
					argc++;
			}
			argv = fxNewLinkerChunk(linker, argc * sizeof(char*));
			for (string = buffer, argc = 1; ; string = NULL, argc++) {
				txString token = strtok(string, " \f\n\r\t\v");
				if (token == NULL)
					break;
				argv[argc] = token;
			}
		}
		c_strcpy(name, "mc");
		includeAddress = &(linker->firstInclude);
		preloadAddress = &(linker->firstPreload);
		resourceAddress = &(linker->firstResource);
		scriptAddress = &(linker->firstScript);
		stripAddress = &(linker->firstStrip);
		for (argi = 1; argi < argc; argi++) {
			if (!c_strcmp(argv[argi], "-1")) {
				optimizing = 1;
			}
			else if (!c_strcmp(argv[argi], "-a")) {
				archiving = 1;
				linker->symbolModulo = creation->nameModulo;
			}
			else if (!c_strcmp(argv[argi], "-b")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-b: no directory");
				base = fxRealDirectoryPath(linker, argv[argi]);
				if (!base)
					fxReportLinkerError(linker, "-b '%s': directory not found", argv[argi]);
			}
			else if (!c_strcmp(argv[argi], "-c")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-c: no creation");
				sscanf(argv[argi], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s", 
					&linker->creation.initialChunkSize,
					&linker->creation.incrementalChunkSize,
					&linker->creation.initialHeapCount,
					&linker->creation.incrementalHeapCount,
					&linker->creation.stackCount,
					&linker->creation.keyCount,
					&linker->creation.nameModulo,
					&linker->creation.symbolModulo,
					&linker->creation.parserBufferSize,
					&linker->creation.parserTableModulo,
					&linker->creation.staticSize,
					linker->main);
				linker->symbolModulo = linker->creation.nameModulo;
			}		
			else if (!c_strcmp(argv[argi], "-o")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-o: no directory");
				output = fxRealDirectoryPath(linker, argv[argi]);
				if (!output)
					fxReportLinkerError(linker, "-o '%s': directory not found", argv[argi]);
			}
			else if (!c_strcmp(argv[argi], "-p")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-p: no module");
				*preloadAddress = fxNewLinkerPreload(linker, argv[argi]);
				preloadAddress = &((*preloadAddress)->nextPreload);
			}
			else if (!c_strcmp(argv[argi], "-r")) {
				argi++;
				c_strncpy(name, argv[argi], sizeof(name));
			}
			else if (!c_strcmp(argv[argi], "-s")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-s: no symbol");
				if (!c_strcmp(argv[argi], "*"))
					linker->stripFlag |= XS_STRIP_IMPLICIT_FLAG;
				else {
					linker->stripFlag |= XS_STRIP_EXPLICIT_FLAG;
					*stripAddress = fxNewLinkerStrip(linker, argv[argi]);
					stripAddress = &((*stripAddress)->nextStrip);
				}
			}
			else if (!c_strcmp(argv[argi], "-u")) {
				argi++;
				if (argi >= argc)
					fxReportLinkerError(linker, "-u: no url");
				url = argv[argi];
				if ((url[0] != '/') && (url[0] != '\\'))
					fxReportLinkerError(linker, "-u: invalid url");
			}
			else {
				input = fxRealFilePath(linker, argv[argi]);
				if (!input)
					fxReportLinkerError(linker, "'%s': file not found", argv[argi]);
				separator = c_strrchr(input, mxSeparator);
				if (!separator)
					fxReportLinkerError(linker, "'%s': invalid extension", input);
				separator = c_strrchr(separator, '.');
				if (!separator)
					fxReportLinkerError(linker, "'%s': invalid extension", input);
				if (!c_strcmp(separator, ".xsb")) {
					*scriptAddress = fxNewLinkerScript(linker, input, &file);
					scriptAddress = &((*scriptAddress)->nextScript);
				}
				else if  (!c_strcmp(separator, ".xsi")) {
					*includeAddress = fxNewLinkerInclude(linker, input);
					includeAddress = &((*includeAddress)->nextInclude);
				}
				else {
					*resourceAddress = fxNewLinkerResource(linker, input, &file);
					resourceAddress = &((*resourceAddress)->nextResource);
				}
			}
		}
		if (!output)
			output = fxRealDirectoryPath(linker, ".");
		if (!base)
			base = output;
			
		size = c_strlen(base);
		script = linker->firstScript;
		while (script) {
			fxBaseScript(linker, script, base, size);
			fxSlashPath(script->path, mxSeparator, url[0]);
			script = script->nextScript;
		}

		linker->symbolTable = fxNewLinkerChunkClear(linker, linker->symbolModulo * sizeof(txLinkerSymbol*));
		if (archiving) {
			resource = linker->firstResource;
			while (resource) {
				fxBaseResource(linker, resource, base, size);
				resource = resource->nextResource;
			}
			script = linker->firstScript;
			while (script) {
				fxMapScript(linker, script);
				script = script->nextScript;
			}
			fxBufferSymbols(linker);
			
			c_strcpy(path, output);
			c_strcat(path, name);
			c_strcat(path, ".xsa");
			fxWriteArchive(linker, path, &file);
		}
		else {
			c_strcpy(path, base);
			c_strcat(path, name);
			c_strcat(path, ".xsi");
			if (fxRealFilePath(linker, path)) {
				fxReadSymbols(linker, path, 0, &file);
				incremental = 1;
			}
			else
				fxDefaultSymbols(linker);
			include = linker->firstInclude;
			while (include) {
				fxReadSymbols(linker, include->path, 1, &file);
				include = include->nextInclude;
			}
			script = linker->firstScript;
			while (script) {
				fxMapScript(linker, script);
				script = script->nextScript;
			}
			fxBufferSymbols(linker);
			fxWriteSymbols(linker, path, &file);
	
			linker->base = url;
			linker->baseLength = c_strlen(url);
	
			creation->nameModulo = linker->creation.nameModulo;
			creation->symbolModulo = linker->creation.symbolModulo;
			the = xsCreateMachine(creation, "xsl", linker);
			mxThrowElse(the);
			fxNewLinkerCallback(the, fx_Function_prototype_bound, "fx_Function_prototype_bound");
				
			xsBeginHost(the);
			{
				xsVars(2);
				{
					xsTry {
						xsCollectGarbage();
						c_strcpy(path, linker->base);
						script = linker->firstScript;
						while (script) {
							c_strcpy(path + linker->baseLength, script->path);
							fxNewNameC(the, path);
							path[linker->baseLength + script->pathSize - 5] = 0;
							fxNewNameC(the, path + linker->baseLength);
							script = script->nextScript;
						}
						preload = linker->firstPreload;
						while (preload) {
							fxSlashPath(preload->name, mxSeparator, url[0]);
							xsResult = xsAwaitImport(preload->name, XS_IMPORT_NAMESPACE);
							xsCollectGarbage();
							preload = preload->nextPreload;
						}
						{
							txSlot* property;
							property = mxBehaviorGetProperty(the, mxAsyncFunctionPrototype.value.reference, mxID(_constructor), XS_NO_ID, XS_OWN);
							property->kind = mxThrowTypeErrorFunction.kind;
							property->value = mxThrowTypeErrorFunction.value;
							property = mxBehaviorGetProperty(the, mxAsyncGeneratorFunctionPrototype.value.reference, mxID(_constructor), XS_NO_ID, XS_OWN);
							property->kind = mxThrowTypeErrorFunction.kind;
							property->value = mxThrowTypeErrorFunction.value;
							property = mxBehaviorGetProperty(the, mxFunctionPrototype.value.reference, mxID(_constructor), XS_NO_ID, XS_OWN);
							property->kind = mxThrowTypeErrorFunction.kind;
							property->value = mxThrowTypeErrorFunction.value;
							property = mxBehaviorGetProperty(the, mxGeneratorFunctionPrototype.value.reference, mxID(_constructor), XS_NO_ID, XS_OWN);
							property->kind = mxThrowTypeErrorFunction.kind;
							property->value = mxThrowTypeErrorFunction.value;
						}
						{
							txSlot* realm = mxModuleInstanceInternal(mxProgram.value.reference)->value.module.realm;
							txSlot* modules = mxRequiredModules(realm)->value.reference;
							txSlot* module = modules->next;
							while (module) {
								mxModuleInstanceInternal(module->value.reference)->value.module.realm = NULL;
								module = module->next;
							}
							mxModuleInstanceInternal(mxProgram.value.reference)->value.module.realm = NULL;
							mxProgram.value.reference = modules; //@@
							mxPendingJobs = mxUndefined;
							mxRunningJobs = mxUndefined;
							mxBreakpoints = mxUndefined;
							mxHostInspectors = mxUndefined;
							mxInstanceInspectors = mxUndefined;
						}
						if (linker->stripFlag) {
							fxFreezeBuiltIns(the);
							mxFunctionInstanceCode(mxThrowTypeErrorFunction.value.reference)->ID = XS_NO_ID; 
							mxFunctionInstanceHome(mxThrowTypeErrorFunction.value.reference)->value.home.object = NULL;
						}
					}
					xsCatch {
						xsStringValue message = xsToString(xsException);
						fxReportLinkerError(linker, "%s", message);
					}
				}
			}
			xsEndHost(the);
			
			if (linker->stripFlag)
				fxStripCallbacks(linker, the);
			else
				fxUnstripCallbacks(linker);
			
			linker->bigintSize = 0;
			count = fxPrepareHeap(the, linker->stripFlag);
			
// 			if (optimizing) {
// 				linker->realm = the;
// 				fxOptimize(linker);
// 				linker->realm = NULL;
// 				xsDeleteMachine(the);
// 				linker->firstCallback = NULL;
// 				linker->firstBuilder = NULL;
// 				linker->lastBuilder = NULL;
// 				linker->builderCount = 0;
// 				linker->firstProjection = NULL;
// 				script = linker->firstScript;
// 				while (script) {
// 					script->builders = C_NULL;
// 					script->callbackNames = C_NULL;
// 					script->hostsCount = 0;
// 					script = script->nextScript;
// 				}
// 				the = xsCreateMachine(creation, "xsl", linker);
// 				mxThrowElse(the);
// 				xsBeginHost(the);
// 				{
// 					xsVars(2);
// 					{
// 						xsTry {
// 							preload = linker->firstPreload;
// 							while (preload) {
// 								fxSlashPath(preload->name, mxSeparator, url[0]);
// 								xsResult = xsCall1(xsGlobal, xsID("require"), xsString(preload->name));
// 								preload = preload->nextPreload;
// 							}
// 							{
// 								txSlot* realm = mxModuleInstanceInternal(mxProgram.value.reference)->value.module.realm;
// 								txSlot* modules = mxRequiredModules(realm)->value.reference;
// 								txSlot* module = modules->next;
// 									fprintf(stderr, "%p %p %p\n", realm, modules, module);
// 								while (module) {
// 								fprintf(stderr, "%p\n", module);
// 									mxModuleInstanceInternal(module->value.reference)->value.module.realm = NULL;
// 									module = modules->next;
// 								}
// 								mxModuleInstanceInternal(mxProgram.value.reference)->value.module.realm = NULL;
// 								mxProgram.value.reference = modules; //@@
// 								mxPendingJobs = mxUndefined;
// 								mxRunningJobs = mxUndefined;
// 								mxBreakpoints = mxUndefined;
// 								mxHostInspectors = mxUndefined;
// 								mxInstanceInspectors = mxUndefined;
// 							}
// 							if (linker->stripping)
// 								fxFreezeBuiltIns(the);
// 							xsCollectGarbage();
// 						}
// 						xsCatch {
// 							xsStringValue message = xsToString(xsException);
// 							fxReportLinkerError(linker, "%s", message);
// 						}
// 					}
// 				}
// 				xsEndHost(the);
// 				if (stripping)
// 					fxStripCallbacks(linker, the);
// 				else
// 					fxUnstripCallbacks(linker);
// 				strip = linker->firstStrip;
// 				while (strip) {
// 					fxStripName(linker, strip->name);
// 					strip = strip->nextStrip;
// 				}
// 				linker->bigintSize = 0;
// 				count = fxPrepareHeap(the, stripping || linker->firstStrip);
// 			}

// 			globalCount = the->stackTop[-1].value.reference->next->value.table.length;
	
			c_strcpy(path, output);
			c_strcat(path, name);
			c_strcat(path, ".xs.h");
 			if (c_stat(path, &headerStat) != 0)
 				incremental = 0;
			
			file = fopen(path, "w");
			fprintf(file, "/* XS GENERATED FILE; DO NOT EDIT! */\n\n");
			fprintf(file, "#include \"xs.h\"\n\n");
		
			fprintf(file, "#ifdef __cplusplus\n");
			fprintf(file, "extern \"C\" {\n");
			fprintf(file, "#endif\n");
			fprintf(file, "#define xsPreparation() xsPreparationAndCreation(NULL)\n");
			fprintf(file, "mxExport void* xsPreparationAndCreation(xsCreation **creation);\n\n");
			script = linker->firstScript;
			while (script) {
				fxWriteScriptExterns(script, file);
				script = script->nextScript;
			}
			fprintf(file, "#ifdef __cplusplus\n");
			fprintf(file, "}\n");
			fprintf(file, "#endif\n");
			fprintf(file, "\n");
	
			fxWriteDefines(linker, file);
			fprintf(file, "\n");

			fclose(file);
			file = NULL;
			
			if (incremental) {
				headerTimes.actime = headerStat.st_atime;
				headerTimes.modtime = headerStat.st_mtime;
				c_utime(path, &headerTimes);
			}
		
			c_strcpy(path, output);
			c_strcat(path, name);
			c_strcat(path, ".xs.c");
			
			
			file = fopen(path, "w");
			fprintf(file, "/* XS GENERATED FILE; DO NOT EDIT! %d */\n\n", ___proto__);
			//fprintf(file, "#pragma GCC diagnostic ignored \"-Wincompatible-pointer-types-discards-qualifiers\"\n");

			fprintf(file, "#include \"xsPlatform.h\"\n");
			fprintf(file, "#define XS_FUNCTION_WEAK\n");
			fprintf(file, "#include \"xsAll.h\"\n");
			fprintf(file, "#include \"%s.xs.h\"\n\n", name);

			fprintf(file, "#ifndef ICACHE_FLASH1_ATTR\n");
			fprintf(file, "#define ICACHE_FLASH1_ATTR ICACHE_FLASH_ATTR\n");
			fprintf(file, "#endif\n");

			fprintf(file, "#define mxAliasCount %d\n", the->aliasCount);
			if (linker->bigintSize) {
				fprintf(file, "#define mxBigIntCount %d\n", (int)linker->bigintSize);
				fprintf(file, "static const txU4 gxBigIntData[mxBigIntCount];\n");
				linker->bigintData = fxNewLinkerChunk(linker, linker->bigintSize * sizeof(txU4));
				linker->bigintSize = 0;
			}
			fprintf(file, "#define mxHeapCount %d\n", (int)count);
			fprintf(file, "static const txSlot gxHeap[mxHeapCount];\n");
			fprintf(file, "#define mxStackCount %ld\n", (long)(the->stackTop - the->stack));
			fprintf(file, "static const txSlot gxStack[mxStackCount];\n");
			fprintf(file, "#define mxKeysCount %d\n", the->keyIndex);
			fprintf(file, "static const txSlot* gxKeys[mxKeysCount];\n");
			fprintf(file, "#define mxNamesCount %d\n", the->nameModulo);
			fprintf(file, "static const txSlot* gxNames[mxNamesCount];\n");
			fprintf(file, "#define mxSymbolsCount %d\n", the->symbolModulo);
			fprintf(file, "static const txSlot* gxSymbols[mxSymbolsCount];\n");
// 			fprintf(file, "#define mxGlobalsCount %d\n", (int)globalCount);
// 			fprintf(file, "static const txSlot* gxGlobals[mxGlobalsCount];\n");
			script = linker->firstScript;
			while (script) {
				if (script->hostsBuffer)
					fprintf(file, "static void xsHostModule%d(xsMachine* the);\n", script->scriptIndex);
				fprintf(file, "static const txU1 gxCode%d[%d];\n", script->scriptIndex, script->codeSize);
				script = script->nextScript;
			}
			fprintf(file, "#define mxScriptsCount %d\n", linker->scriptCount);
			fprintf(file, "static const txScript gxScripts[mxScriptsCount];\n");		
			fprintf(file, "static const txPreparation gxPreparation;\n\n");
		
			if (linker->stripFlag) {
				fprintf(file, "static void fxDeadStrip(txMachine* the) { mxUnknownError(\"dead strip\"); }\n\n");
				fxPrintBuilders(the, file);
			}
			script = linker->firstScript;
			while (script) {
				fxWriteScriptHosts(script, file);
				fxWriteScriptCode(script, file);
				script = script->nextScript;
			}
		
			fprintf(file, "static const txScript gxScripts[mxScriptsCount] = {\n");
			script = linker->firstScript;
			while (script) {
				fxWriteScriptRecord(script, file);
				script = script->nextScript;
				if (script)
					fprintf(file, ",\n");
			}
			fprintf(file, "\n};\n\n");
		
			fprintf(file, "static const txSlot gxHeap[mxHeapCount] = {\n");
			fxPrintHeap(the, file, count);
			fprintf(file, "};\n\n");
			fprintf(file, "static const txSlot gxStack[mxStackCount] = {\n");
			fxPrintStack(the, file);
			fprintf(file, "};\n\n");
			if (linker->bigintSize) {
				count = 0;
				fprintf(file, "static const txU4 gxBigIntData[mxBigIntCount] = {");
				while (count < linker->bigintSize) {
					if (count % 8)
						fprintf(file, " ");
					else
						fprintf(file, "\n\t");
					fprintf(file, "0x%8.8X,", linker->bigintData[count]);
					count++;
				}
				fprintf(file, "\n};\n\n");
			}
// 			fprintf(file, "static const txSlot* gxGlobals[mxGlobalsCount] ICACHE_FLASH1_ATTR = {\n");
// 			fxPrintTable(the, file, globalCount, the->stackTop[-1].value.reference->next->value.table.address);
// 			fprintf(file, "};\n\n");
			fprintf(file, "static const txSlot* gxKeys[mxKeysCount] ICACHE_FLASH1_ATTR = {\n");
			fxPrintTable(the, file, the->keyIndex, the->keyArray);
			fprintf(file, "};\n\n");
			fprintf(file, "static const txSlot* gxNames[mxNamesCount] ICACHE_FLASH1_ATTR = {\n");
			fxPrintTable(the, file, the->nameModulo, the->nameTable);
			fprintf(file, "};\n\n");
			fprintf(file, "static const txSlot* gxSymbols[mxSymbolsCount] ICACHE_FLASH1_ATTR = {\n");
			fxPrintTable(the, file, the->symbolModulo, the->symbolTable);
			fprintf(file, "};\n\n");
			fprintf(file, "static const txPreparation gxPreparation = {\n");
			fprintf(file, "\t{ XS_MAJOR_VERSION, XS_MINOR_VERSION, XS_PATCH_VERSION, 0 },\n");
			fprintf(file, "\tmxAliasCount,\n");
			fprintf(file, "\tmxHeapCount,\n");
			fprintf(file, "\t(txSlot*)gxHeap,\n");
			fprintf(file, "\tmxStackCount,\n");
			fprintf(file, "\t(txSlot*)gxStack,\n");
			fprintf(file, "\tmxKeysCount,\n");
			fprintf(file, "\t(txSlot**)gxKeys,\n");
			fprintf(file, "\tmxNamesCount,\n");
			fprintf(file, "\t(txSlot**)gxNames,\n");
			fprintf(file, "\tmxSymbolsCount,\n");
			fprintf(file, "\t(txSlot**)gxSymbols,\n");
			fprintf(file, "\t%d,\n", (int)linker->baseLength);
			fprintf(file, "\t");
			fxWriteCString(file, linker->base);
			fprintf(file, ",\n");
			fprintf(file, "\tmxScriptsCount,\n");
			fprintf(file, "\t(txScript*)gxScripts,\n");
			fprintf(file, "\t{ %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },\n",
				linker->creation.initialChunkSize,
				linker->creation.incrementalChunkSize,
				linker->creation.initialHeapCount,
				linker->creation.incrementalHeapCount,
				linker->creation.stackCount,
				linker->creation.keyCount,
				linker->creation.nameModulo,
				linker->creation.symbolModulo,
				linker->creation.parserBufferSize,
				linker->creation.parserTableModulo,
				linker->creation.staticSize
			);
			fprintf(file, "\t\"%s\",\n", linker->main);
			fprintf(file, "\t{ 0x%.2X", linker->symbolsChecksum[0]);
			for (argi = 1; argi < 16; argi++)
				fprintf(file, ", 0x%.2X", linker->symbolsChecksum[argi]);
			fprintf(file, " }\n");
			fprintf(file, "};\n");
			fprintf(file, "void* xsPreparationAndCreation(xsCreation **creation) { if (creation) *creation = (xsCreation *)&gxPreparation.creation; return (void*)&gxPreparation; }\n\n");

			if (linker->stripFlag)
				fxStripDefaults(linker, file);
			else
				fprintf(file, "#include \"xsDefaults.c\"\n\n");
		
			fclose(file);
			file = NULL;
		
		}
	}
	else {
		if (linker->error != C_EINVAL) {
		#if mxWindows
			char buffer[2048];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, linker->error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, sizeof(buffer), NULL);
			fprintf(stderr, "### %s\n", buffer);
		#else
			fprintf(stderr, "### %s\n", strerror(linker->error));
		#endif
		}
	}
	if (file)
		fclose(file);
// 	if (the)
// 		xsDeleteMachine(the);
	fxTerminateLinker(linker);
	return linker->error;
}

/* KEYS */

void fxBuildKeys(txMachine* the)
{
	txLinker* linker = (txLinker*)(the->context);
	txID c = linker->symbolIndex, i;
	for (i = 0; i < XS_SYMBOL_ID_COUNT; i++) {
		txLinkerSymbol* symbol = linker->symbolArray[i];
		txID id = the->keyIndex;
		txSlot* description = fxNewSlot(the);
		fxCopyStringC(the, description, symbol->string);
		the->keyArray[id] = description;
		the->keyIndex++;
	}
	for (; i < c; i++) {
		txLinkerSymbol* symbol = linker->symbolArray[i];
		fxNewNameX(the, symbol->string);
	}
}

/* MODULES */

txID fxFindModule(txMachine* the, txSlot* realm, txID moduleID, txSlot* slot)
{
	txLinker* linker = (txLinker*)(the->context);
	char name[C_PATH_MAX];
	char path[C_PATH_MAX];
	char separator;
	txBoolean absolute = 0, relative = 0, search = 0;
	txInteger dot = 0;
	txSlot *key;
	txString slash;
	txID id;
		
	fxToStringBuffer(the, slot, name, sizeof(name));
	if (!c_strncmp(name, "/", 1)) {
		absolute = 1;
	}	
	else if (!c_strncmp(name, "./", 2)) {
		dot = 1;
		relative = 1;
	}	
	else if (!c_strncmp(name, "../", 3)) {
		dot = 2;
		relative = 1;
	}
	else {
		relative = 1;
		search = 1;
	}
	separator = linker->base[0];
	fxSlashPath(name, '/', separator);
	slash = c_strrchr(name, separator);
	if (!slash)
		slash = name;
	slash = c_strrchr(slash, '.');
	if (!slash)
		c_strcat(name, ".xsb");
	if (absolute) {
		c_strcpy(path, linker->base);
		c_strcat(path, name + 1);
		if (fxFindScript(the, path, &id))
			return id;
	}
	if (relative && (moduleID != XS_NO_ID)) {
		key = fxGetKey(the, moduleID);
		c_strcpy(path, key->value.key.string);
		slash = c_strrchr(path, separator);
		if (!slash)
			return XS_NO_ID;
		if (dot == 0)
			slash++;
		else if (dot == 2) {
			*slash = 0;
			slash = c_strrchr(path, separator);
			if (!slash)
				return XS_NO_ID;
		}
		if (!c_strncmp(path, linker->base, linker->baseLength)) {
			*slash = 0;
			c_strcat(path, name + dot);
			if (fxFindScript(the, path, &id))
				return id;
		}
	}
	if (search) {
		c_strcpy(path, linker->base);
		c_strcat(path, name);
		if (fxFindScript(the, path, &id))
			return id;
	}
	return XS_NO_ID;
}

txBoolean fxFindScript(txMachine* the, txString path, txID* id)
{
	txLinker* linker = (txLinker*)(the->context);
	txLinkerScript* linkerScript = linker->firstScript;
	path += linker->baseLength;
	while (linkerScript) {
		if (!c_strcmp(path, linkerScript->path)) {
			path -= linker->baseLength;
			*id = fxNewNameC(the, path);
			return 1;
		}
		linkerScript = linkerScript->nextScript;
	}
	*id = XS_NO_ID;
	return 0;
}

void fxFreezeBuiltIn(txMachine* the)
{
	mxPushInteger(1);
	mxPush(mxObjectConstructor);
	fxCallID(the, mxID(_freeze));
	mxPop();
}

void fxFreezeBuiltIns(txMachine* the)
{
	txInteger index;
	const txTypeDispatch *dispatch;
	
	mxPush(mxAtomicsObject); fxFreezeBuiltIn(the);
	mxPush(mxJSONObject); fxFreezeBuiltIn(the);
	mxPush(mxMathObject); fxFreezeBuiltIn(the);
	mxPush(mxReflectObject); fxFreezeBuiltIn(the);

	mxPush(mxArgumentsSloppyPrototype); fxFreezeBuiltIn(the);
	mxPush(mxArgumentsStrictPrototype); fxFreezeBuiltIn(the);
	mxPush(mxArrayBufferPrototype); fxFreezeBuiltIn(the);
	mxPush(mxArrayIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxArrayPrototype); fxFreezeBuiltIn(the);
	mxPush(mxAsyncFromSyncIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxAsyncFunctionPrototype); fxFreezeBuiltIn(the);
	mxPush(mxAsyncGeneratorFunctionPrototype); fxFreezeBuiltIn(the);
	mxPush(mxAsyncGeneratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxAsyncIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxBigIntPrototype); fxFreezeBuiltIn(the);
	mxPush(mxBooleanPrototype); fxFreezeBuiltIn(the);
	mxPush(mxCompartmentPrototype); fxFreezeBuiltIn(the);
	mxPush(mxDataViewPrototype); fxFreezeBuiltIn(the);
	mxPush(mxDatePrototype); fxFreezeBuiltIn(the);
	mxPush(mxErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxEvalErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxFunctionPrototype); fxFreezeBuiltIn(the);
	mxPush(mxGeneratorFunctionPrototype); fxFreezeBuiltIn(the);
	mxPush(mxGeneratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxHostPrototype); fxFreezeBuiltIn(the);
	mxPush(mxIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxMapEntriesIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxMapKeysIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxMapPrototype); fxFreezeBuiltIn(the);
	mxPush(mxMapValuesIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxModulePrototype); fxFreezeBuiltIn(the);
	mxPush(mxNumberPrototype); fxFreezeBuiltIn(the);
	mxPush(mxObjectPrototype); fxFreezeBuiltIn(the);
	mxPush(mxPromisePrototype); fxFreezeBuiltIn(the);
	mxPush(mxProxyPrototype); fxFreezeBuiltIn(the);
	mxPush(mxRangeErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxReferenceErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxRegExpPrototype); fxFreezeBuiltIn(the);
	mxPush(mxRegExpStringIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSetEntriesIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSetKeysIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSetPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSetValuesIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSharedArrayBufferPrototype); fxFreezeBuiltIn(the);
	mxPush(mxStringIteratorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxStringPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSymbolPrototype); fxFreezeBuiltIn(the);
	mxPush(mxSyntaxErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxTransferPrototype); fxFreezeBuiltIn(the);
	mxPush(mxTypedArrayPrototype); fxFreezeBuiltIn(the);
	mxPush(mxTypeErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxURIErrorPrototype); fxFreezeBuiltIn(the);
	mxPush(mxWeakMapPrototype); fxFreezeBuiltIn(the);
	mxPush(mxWeakSetPrototype); fxFreezeBuiltIn(the);
	
	for (index = 0, dispatch = &gxTypeDispatches[0]; index < mxTypeArrayCount; index++, dispatch++) {
		mxPush(the->stackPrototypes[-1 - dispatch->constructorID]);
		fxGetID(the, mxID(_prototype));
		fxFreezeBuiltIn(the);
	}
	mxPush(mxEnumeratorFunction); fxGetID(the, mxID(_prototype)); fxFreezeBuiltIn(the);
	
	mxPush(mxArrayPrototype); fxGetID(the, mxID(_Symbol_unscopables)); fxFreezeBuiltIn(the);
	
	mxPush(mxProgram); fxFreezeBuiltIn(the); //@@
	
}

void fxLoadModule(txMachine* the, txSlot* realm, txID moduleID)
{
	txSlot* key = fxGetKey(the, moduleID);
 	char buffer[C_PATH_MAX];
 	txString path = buffer;
 	c_strcpy(path, key->value.key.string);
 	txScript* script = fxLoadScript(the, path);
	fxResolveModule(the, realm, moduleID, script, C_NULL, C_NULL);
}

txScript* fxLoadScript(txMachine* the, txString path)
{
	txLinker* linker = (txLinker*)(the->context);
	txLinkerScript* linkerScript = linker->firstScript;
	path += linker->baseLength;
	while (linkerScript) {
		if (!c_strcmp(path, linkerScript->path)) {
			txScript* script = fxNewLinkerChunk(linker, sizeof(txScript));
			script->callback = fxLinkerScriptCallback;
			script->symbolsBuffer = NULL;
			script->symbolsSize = 0;
			script->codeBuffer = linkerScript->codeBuffer;
			script->codeSize = linkerScript->codeSize;
			script->hostsBuffer = NULL;
			script->hostsSize = 0;
			script->path = linkerScript->path;
			script->version[0] = XS_MAJOR_VERSION;
			script->version[1] = XS_MINOR_VERSION;
			script->version[2] = XS_PATCH_VERSION;
			script->version[3] = 0;
			return script;
		}
		linkerScript = linkerScript->nextScript;
	}
	return C_NULL;
}

txScript* fxParseScript(txMachine* the, void* stream, txGetter getter, txUnsigned flags)
{
	return C_NULL;
}

/* DEBUG */

void fxCreateMachinePlatform(txMachine* the)
{
#ifdef mxDebug
	the->connection = mxNoSocket;
#endif
	the->host = NULL;
	the->fakeCallback = (txCallback)1;
}

void fxDeleteMachinePlatform(txMachine* the)
{
}

#ifdef mxDebug

void fxAbort(txMachine* the)
{
	c_exit(0);
}

void fxConnect(txMachine* the)
{
	char name[256];
	char* colon;
	int port;
	struct sockaddr_in address;
#if mxWindows
	if (GetEnvironmentVariable("XSBUG_HOST", name, sizeof(name))) {
#else
	colon = getenv("XSBUG_HOST");
	if ((colon) && (c_strlen(colon) + 1 < sizeof(name))) {
		c_strcpy(name, colon);
#endif		
		colon = strchr(name, ':');
		if (colon == NULL)
			port = 5002;
		else {
			*colon = 0;
			colon++;
			port = strtol(colon, NULL, 10);
		}
	}
	else {
		strcpy(name, "localhost");
		port = 5002;
	}
	memset(&address, 0, sizeof(address));
  	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(name);
	if (address.sin_addr.s_addr == INADDR_NONE) {
		struct hostent *host = gethostbyname(name);
		if (!host)
			return;
		memcpy(&(address.sin_addr), host->h_addr, host->h_length);
	}
  	address.sin_port = htons(port);
#if mxWindows
{  	
	WSADATA wsaData;
	unsigned long flag;
	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR)
		return;
	the->connection = socket(AF_INET, SOCK_STREAM, 0);
	if (the->connection == INVALID_SOCKET)
		return;
  	flag = 1;
  	ioctlsocket(the->connection, FIONBIO, &flag);
	if (connect(the->connection, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
		if (WSAEWOULDBLOCK == WSAGetLastError()) {
			fd_set fds;
			struct timeval timeout = { 2, 0 }; // 2 seconds, 0 micro-seconds
			FD_ZERO(&fds);
			FD_SET(the->connection, &fds);
			if (select(0, NULL, &fds, NULL, &timeout) == 0)
				goto bail;
			if (!FD_ISSET(the->connection, &fds))
				goto bail;
		}
		else
			goto bail;
	}
 	flag = 0;
 	ioctlsocket(the->connection, FIONBIO, &flag);
}
#else
{  	
	int	flag;
	the->connection = socket(AF_INET, SOCK_STREAM, 0);
	if (the->connection <= 0)
		goto bail;
	c_signal(SIGPIPE, SIG_IGN);
#if mxMacOSX
	{
		int set = 1;
		setsockopt(the->connection, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	}
#endif
	flag = fcntl(the->connection, F_GETFL, 0);
	fcntl(the->connection, F_SETFL, flag | O_NONBLOCK);
	if (connect(the->connection, (struct sockaddr*)&address, sizeof(address)) < 0) {
    	 if (errno == EINPROGRESS) { 
			fd_set fds;
			struct timeval timeout = { 2, 0 }; // 2 seconds, 0 micro-seconds
			int error = 0;
			unsigned int length = sizeof(error);
			FD_ZERO(&fds);
			FD_SET(the->connection, &fds);
			if (select(the->connection + 1, NULL, &fds, NULL, &timeout) == 0)
				goto bail;
			if (!FD_ISSET(the->connection, &fds))
				goto bail;
			if (getsockopt(the->connection, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
				goto bail;
			if (error)
				goto bail;
		}
		else
			goto bail;
	}
	fcntl(the->connection, F_SETFL, flag);
	c_signal(SIGPIPE, SIG_DFL);
}
#endif
	return;
bail:
	fxDisconnect(the);
}

void fxDisconnect(txMachine* the)
{
#if mxWindows
	if (the->connection != INVALID_SOCKET) {
		closesocket(the->connection);
		the->connection = INVALID_SOCKET;
	}
	WSACleanup();
#else
	if (the->connection >= 0) {
		close(the->connection);
		the->connection = -1;
	}
#endif
}

txBoolean fxIsConnected(txMachine* the)
{
	return (the->connection != mxNoSocket) ? 1 : 0;
}

txBoolean fxIsReadable(txMachine* the)
{
	return 0;
}

void fxReceive(txMachine* the)
{
	int count;
	if (the->connection != mxNoSocket) {
#if mxWindows
		count = recv(the->connection, the->debugBuffer, sizeof(the->debugBuffer) - 1, 0);
		if (count < 0)
			fxDisconnect(the);
		else
			the->debugOffset = count;
#else
	again:
		count = read(the->connection, the->debugBuffer, sizeof(the->debugBuffer) - 1);
		if (count < 0) {
			if (errno == EINTR)
				goto again;
			else
				fxDisconnect(the);
		}
		else
			the->debugOffset = count;
#endif
	}
	the->debugBuffer[the->debugOffset] = 0;
}

void fxSend(txMachine* the, txBoolean more)
{
	if (the->connection != mxNoSocket) {
#if mxWindows
		if (send(the->connection, the->echoBuffer, the->echoOffset, 0) <= 0)
			fxDisconnect(the);
#else
	again:
		if (write(the->connection, the->echoBuffer, the->echoOffset) <= 0) {
			if (errno == EINTR)
				goto again;
			else
				fxDisconnect(the);
		}
#endif
	}
}

#endif /* mxDebug */


