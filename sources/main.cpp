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
	string params;
	string value;
};

std::map<string, Replacement> replacements;
std::vector<string> extensionsWhitelist = { ".h", ".hpp", ".c", ".cpp" };
string command;
bool overwrite = false;

string convertCompilerPath(const string &path)
{
#ifdef CAGE_SYSTEM_WINDOWS
	return stringizer() + "\"" + replace(path, "/", "\\") + "\"";
#else
	return replace(path, " ", "\\ ");
#endif // CAGE_SYSTEM_WINDOWS
}

void loadConfiguration()
{
	Holder<Ini> ini = configGenerateIni("expmac");

#ifdef CAGE_SYSTEM_WINDOWS
	const string cmdArgs = "/E";
#else
	const string cmdArgs = "-E";
#endif // CAGE_SYSTEM_WINDOWS
	command = convertCompilerPath(ini->getString("compiler", "path")) + " " + ini->getString("compiler", "arguments", cmdArgs) + " ";
	CAGE_LOG(SeverityEnum::Info, "expmac", stringizer() + "compiler command: " + command);

	const auto exts = ini->values("extensions");
	if (!exts.empty())
	{
		extensionsWhitelist.clear();
		for (const string &s : exts)
			extensionsWhitelist.push_back(s);
	}
	for (const string &e : extensionsWhitelist)
		CAGE_LOG(SeverityEnum::Info, "expmac", stringizer() + "whitelisted extension: " + e);
}

void loadReplacements(const string &path)
{
	Holder<Ini> ini = newIni();
	ini->importFile(path);
	for (const string &s : ini->sections())
	{
		const string a = ini->getString(s, "macro");
		const string b = ini->getString(s, "params");
		const string c = replace(ini->getString(s, "value"), "$", "#");
		if (replacements.find(a) != replacements.end())
		{
			CAGE_LOG_THROW(stringizer() + "macro: " + a);
			CAGE_THROW_ERROR(Exception, "duplicate macro name");
		}
		replacements[a] = { b, c };
	}
	ini->checkUnused();
	if (replacements.empty())
		CAGE_THROW_ERROR(Exception, "no macros loaded");
}

bool testWhitelisted(const string &path)
{
	const string ext = toLower(pathExtractExtension(path));
	for (const auto &w : extensionsWhitelist)
		if (ext == w)
			return true;
	return false;
}

string convertLine(const string line, const std::pair<const string, Replacement> &replacement)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", stringizer() + "converting line: " + line);

	const string tmpName = stringizer() + currentThreadId() + ".tmp";
	Holder<File> f = writeFile(tmpName);
	f->writeLine(stringizer() + "#define " + replacement.first + replacement.second.params + " " + replacement.second.value);
	f->writeLine(line);
	f->close();

	ProcessCreateConfig cfg(command + tmpName);
	cfg.discardStdErr = true;
	Holder<Process> p = newProcess(cfg);
	if (p->wait() != 0)
		CAGE_THROW_ERROR(Exception, "compiler processing returned error");
	auto res = p->readAll();
	Holder<LineReader> lr = newLineReader(res);
	string l, out;
	lr->readLine(l); // skip first line - the one with #define
	while (lr->readLine(l))
	{
		if (isPattern(l, "#line", "", ""))
			continue;
		out += l;
	}

	return out;
}

string processLine(const string line)
{
	for (const auto &it : replacements)
		if (isPattern(line, "", it.first, ""))
			return convertLine(line, it);
	return line;
}

void processFile(const string &path)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", stringizer() + "processing file: " + path);
	
	if (!testWhitelisted(path))
	{
		CAGE_LOG(SeverityEnum::Info, "expmac", "extension not whitelisted - skipping the file");
		return;
	}

	Holder<File> input = readFile(path);
	Holder<File> output = writeFile(path + ".replacement");

	string line;
	while (input->readLine(line))
	{
		line = processLine(line);
		output->writeLine(line);
	}

	input->close();
	output->close();
	if (overwrite)
		pathMove(path + ".replacement", path);

	CAGE_LOG(SeverityEnum::Info, "expmac", "file done");
}

void processPath(const string &path);

void processDirectory(const string &path)
{
	CAGE_LOG(SeverityEnum::Info, "expmac", stringizer() + "processing directory: " + path);
	Holder<DirectoryList> list = newDirectoryList(path);
	while (list->valid())
	{
		processPath(list->fullPath());
		list->next();
	}
}

void processPath(const string &path)
{
	const PathTypeFlags flags = pathType(path);
	if (any(flags & PathTypeFlags::File))
		return processFile(path);
	if (any(flags & (PathTypeFlags::Directory | PathTypeFlags::Archive)))
		return processDirectory(path);
	CAGE_LOG_THROW(stringizer() + "path: " + path);
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

		loadReplacements(ini->cmdString('r', "replacements", "replacements.ini"));

		auto paths = ini->cmdArray(0, "--");
		if (paths.empty())
			CAGE_THROW_ERROR(Exception, "no paths");

		ini->checkUnused();
		ini.clear();

		for (const string &p : paths)
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
