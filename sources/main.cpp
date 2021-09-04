#include <cage-core/logger.h>
#include <cage-core/process.h>
#include <cage-core/config.h>
#include <cage-core/files.h>
#include <cage-core/ini.h>
#include <cage-core/string.h>
#include <cage-core/concurrent.h>
#include <cage-core/lineReader.h>

#include <map>
#include <vector>

using namespace cage;

struct Replacement
{
	String params;
	String value;
};

std::map<String, Replacement> replacements;
std::vector<String> extensionsWhitelist = { ".h", ".hpp", ".c", ".cpp" };
String command;
bool overwrite = false;

String convertCompilerPath(const String &path)
{
#ifdef CAGE_SYSTEM_WINDOWS
	return Stringizer() + "\"" + replace(path, "/", "\\") + "\"";
#else
	return replace(path, " ", "\\ ");
#endif // CAGE_SYSTEM_WINDOWS
}

void loadConfiguration()
{
	Holder<Ini> ini = configGenerateIni("expmac");

#ifdef CAGE_SYSTEM_WINDOWS
	const String cmdArgs = "/E";
#else
	const String cmdArgs = "-E";
#endif // CAGE_SYSTEM_WINDOWS
	command = convertCompilerPath(ini->getString("compiler", "path")) + " " + ini->getString("compiler", "arguments", cmdArgs) + " ";
	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "compiler command: " + command);

	const auto exts = ini->values("extensions");
	if (!exts.empty())
	{
		extensionsWhitelist.clear();
		for (const String &s : exts)
			extensionsWhitelist.push_back(s);
	}
	for (const String &e : extensionsWhitelist)
		CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "whitelisted extension: " + e);
}

void loadReplacements(const String &path)
{
	Holder<Ini> ini = newIni();
	ini->importFile(path);
	for (const String &s : ini->sections())
	{
		const String a = ini->getString(s, "macro");
		const String b = ini->getString(s, "params");
		const String c = replace(ini->getString(s, "value"), "$", "#");
		if (replacements.find(a) != replacements.end())
		{
			CAGE_LOG_THROW(Stringizer() + "macro: " + a);
			CAGE_THROW_ERROR(Exception, "duplicate macro name");
		}
		replacements[a] = { b, c };
	}
	ini->checkUnused();
	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "loaded " + replacements.size() + " replacements");
	if (replacements.empty())
		CAGE_THROW_ERROR(Exception, "no macros loaded");
}

bool testWhitelisted(const String &path)
{
	const String ext = toLower(pathExtractExtension(path));
	for (const auto &w : extensionsWhitelist)
		if (ext == w)
			return true;
	return false;
}

String convertLine(const String line, const std::pair<const String, Replacement> &replacement)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "converting line: " + line);

	const String tmpName = Stringizer() + currentThreadId() + ".tmpline";
	Holder<File> f = writeFile(tmpName);
	f->writeLine(Stringizer() + "#define " + replacement.first + replacement.second.params + " " + replacement.second.value);
	f->writeLine(line);
	f->close();

	ProcessCreateConfig cfg(command + tmpName);
	cfg.discardStdErr = true;
	Holder<Process> p = newProcess(cfg);
	if (p->wait() != 0)
		CAGE_THROW_ERROR(Exception, "compiler processing returned error");
	pathRemove(tmpName);
	auto res = p->readAll();
	Holder<LineReader> lr = newLineReader(res);
	String l, out;
	lr->readLine(l); // skip first line - the one with #define
	while (lr->readLine(l))
	{
		if (isPattern(l, "#line", "", ""))
			continue;
		out += l;
	}

	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "line converted to: " + out);
	return out;
}

bool lineIsPreprocessor(String line)
{
	line = trim(line);
	return !line.empty() && line[0] == '#';
}

String processLine(String line)
{
	if (lineIsPreprocessor(line))
		return line;
	for (const auto &it : replacements)
		if (isPattern(line, "", it.first, ""))
			line = convertLine(line, it);
	return line;
}

void processFile(const String &path)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "processing file: " + path);
	
	if (!testWhitelisted(path))
	{
		CAGE_LOG(SeverityEnum::Info, "expmac", "extension not whitelisted - skipping the file");
		return;
	}

	const String tmpName = overwrite ? Stringizer() + currentThreadId() + ".tmpfile" : path + ".replacement";
	Holder<File> input = readFile(path);
	Holder<File> output = writeFile(tmpName);

	String line;
	while (input->readLine(line))
	{
		line = processLine(line);
		output->writeLine(line);
	}

	input->close();
	output->close();

	if (overwrite)
	{
		pathRemove(path);
		pathMove(tmpName, path);
	}

	CAGE_LOG(SeverityEnum::Info, "expmac", "file done");
}

void processPath(const String &path);

void processDirectory(const String &path)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", Stringizer() + "processing directory: " + path);
	Holder<DirectoryList> list = newDirectoryList(path);
	while (list->valid())
	{
		processPath(list->fullPath());
		list->next();
	}
}

void processPath(const String &path)
{
	const PathTypeFlags flags = pathType(path);
	if (any(flags & PathTypeFlags::File))
		return processFile(path);
	if (any(flags & (PathTypeFlags::Directory | PathTypeFlags::Archive)))
		return processDirectory(path);
	CAGE_LOG_THROW(Stringizer() + "path: " + path);
	CAGE_THROW_ERROR(Exception, "invalid path");
}

int main(int argc, const char *args[])
{
	Holder<Logger> log = newLogger();
	log->format.bind<logFormatConsole>();
	log->output.bind<logOutputStdOut>();

	try
	{
		loadConfiguration();
		Holder<Ini> ini = newIni();
		ini->parseCmd(argc, args);

		overwrite = ini->cmdBool('o', "overwrite", false);
		if (overwrite)
			CAGE_LOG(SeverityEnum::Info, "expmac", "input files will be overwritten");
		else
			CAGE_LOG(SeverityEnum::Info, "expmac", "replacement files will be created next to original files");

		loadReplacements(ini->cmdString('r', "replacements", "replacements.ini"));

		auto paths = ini->cmdArray(0, "--");
		if (paths.empty())
			CAGE_THROW_ERROR(Exception, "no paths");

		ini->checkUnused();
		ini.clear();

		for (const String &p : paths)
			processPath(pathToAbs(p));

		CAGE_LOG(SeverityEnum::Info, "expmac", "all done");
		return 0;
	}
	catch (...)
	{
		detail::logCurrentCaughtException();
	}
	return 1;
}
