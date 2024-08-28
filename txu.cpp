//---------------------------------------------------------------
// txu.cpp
// A program to convert text files between the ANSI, UTF-8, and
// UTF-16 character formats.
//
// (C) Copyright 2011 Ammon R. Campbell.
// You may use this program freely for non-commercial purposes
// provided you do so entirely at your own risk.  Commercial
// use is not permitted without the author's express consent.
//---------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>
#include <tchar.h>
#include <stdlib.h>
#include <vector>
#include <string>

// Global variables.
bool   bVerbose = false;   // True if verbose output enabled.
size_t nLines = 0;         // Total number of lines read.
size_t nChars = 0;         // Total number of characters read.

// Type to indicate one of several possible encodings for a text file.
enum TxEncoding
{
   FMT_UNKNOWN = 0,        // Encoding is not known, not initialized, error, etc.
   FMT_AUTO,               // An attempt should be made to automatically
                           // determine the encoding of the text.
   FMT_ANSI,               // Old fashioned 8-bit ANSI ASCII text.
   FMT_UTF8,               // UTF-8 encoding.  The width of a character varies.
   FMT_UTF16,              // UTF-16 little endian encoding.
   FMT_UTF16BE             // UTF-16 big endian encoding.
};

//----------------------------------------------------------
// Retrieve human-readable name of TxEncoding value.
//----------------------------------------------------------
const _TCHAR * TxEncodingToName(TxEncoding t)
{
   if (t == FMT_AUTO)      return _T("AUTO");
   if (t == FMT_ANSI)      return _T("ANSI");
   if (t == FMT_UTF8)      return _T("UTF8");
   if (t == FMT_UTF16)     return _T("UTF16");
   if (t == FMT_UTF16BE)   return _T("UTF16BE");
   return _T("UNKNOWN");
}

//----------------------------------------------------------
// Retrieve TxEncoding for the given name.
//----------------------------------------------------------
TxEncoding TxEncodingFromName(const _TCHAR *t)
{
   if (_tcsicmp(t, _T("AUTO")) == 0)      return FMT_AUTO;
   if (_tcsicmp(t, _T("ANSI")) == 0)      return FMT_ANSI;
   if (_tcsicmp(t, _T("UTF8")) == 0)      return FMT_UTF8;
   if (_tcsicmp(t, _T("UTF16")) == 0)     return FMT_UTF16;
   if (_tcsicmp(t, _T("UTF16BE")) == 0)   return FMT_UTF16BE;
   return FMT_UNKNOWN;
}

//----------------------------------------------------------
// msg:
// Output message to console in consistent format.
//----------------------------------------------------------
static void msg(const _TCHAR *msg1, const _TCHAR *msg2=NULL)
{
   _ftprintf(stderr, _T("txu:  %s"), msg1);
   if (msg2 != NULL && msg2[0] != '\0')
      _ftprintf(stderr, _T(":  %s"), msg2);
   _ftprintf(stderr, _T("\n"));
}

//----------------------------------------------------------
// OptionNameIs:
// Checks the name of a command line option.
//
// Returns true if specified command line option matches the
// specified name; false otherwise.
//----------------------------------------------------------
static bool OptionNameIs(
   const _TCHAR *szArg,    // Command line argument to be examined.
   const _TCHAR *szName    // Name to compare to command line option string.
   )
{
   // Check for bogus arguments.
   if (szArg == NULL || szArg[0] == '\0' || szName == NULL || szName[0] == '\0')
      return false;

   // If the command line argument is preceeded by a '-' or '/'
   // switch indicator, then skip the first character.
   if (szArg[0] == '-' || szArg[0] == '/')
      szArg++;

   // Compare the specified name with the argument string.
   while (*szArg && *szName)
   {
      // Does this character match?
      if (tolower(*szName++) != tolower(*szArg++))
         return false;
   }

   // If the argument string ended before the name string, then
   // bail.
   if (*szName)
      return false;

   // If the next character of the argument string is alphanumeric,
   // then bail.
   _TCHAR c = *szArg;
   if (isalpha(c) || isdigit(c) || *szArg == '_')
      return false;

   // We have a match.
   return true;
}

//----------------------------------------------------------
// OptionValue:
// Retrieves a pointer to the value portion of a command
// line option.  The input string is assumed to be of the
// form OPTIONNAME=OPTIONVALUE
//
// Returns pointer to value portion of szArg if successful.
// Returns pointer to empty string if error occurs.
//----------------------------------------------------------
static const _TCHAR * OptionValue(
   const _TCHAR *szArg  // Pointer to command line argument string to be examined.
   )
{
   static _TCHAR* empty = _T("");
   static _TCHAR p[1024] = _T("");

   if (szArg == NULL)
      return empty;

   while (*szArg && *szArg != '=' && *szArg != ':')
      szArg++;

   if (*szArg == '=' || *szArg == ':')
   {
      szArg++; // Skip '=' or ':'

      // Remove leading quote (if any).
      if (*szArg == '"' && szArg[1] != '\0')
         szArg++;

      // Copy the rest of the string.
      _tcscpy_s(p, 1024, szArg);

      // Remove trailing spaces (if any).
      while (_tcslen(p) > 0 && p[_tcslen(p) - 1] == ' ' || p[_tcslen(p) - 1] == '\t')
         p[_tcslen(p) - 1] = '\0';

      // Remove trailing quote (if any).
      if (_tcslen(p) > 1 && p[_tcslen(p) - 1] == '"')
         p[_tcslen(p) - 1] = '\0';

      // Caller gets pointer to string buffer.
      return &p[0];
   }

   return empty;
}

//----------------------------------------------------------
// Show command line usage information for the program.
//----------------------------------------------------------
static void Usage(void)
{
   printf("Usage:  txu [options] infile [outfile]\n");
   printf("\n");
   printf("  Reads infile and writes output to stdout, or to outfile if given.\n");
   printf("  Note that UTF <-> ANSI conversions always use US/ANSI code page.\n");
   printf("\n");
   printf("Options:\n");
   printf("  /INFORMAT=f   Specify format of input file, where 'f' is one of\n");
   printf("                AUTO, ANSI, UTF8, UTF16, UTF16BE.  Default AUTO.\n");
   printf("  /OUTFORMAT=f  Specify format of output file, where 'f' is one of\n");
   printf("                ANSI, UTF8, UTF16, UTF16BE.  Default ANSI.\n");
   printf("  /VERBOSE      Verbose output to stderr.  Useful for debugging.\n");
}

//----------------------------------------------------------
// Reads one character from the given file assuming the
// given encoding, placing the result into 'Char'.
// Returns true if successful, false if end of file.
//----------------------------------------------------------
static bool ReadChar(FILE *fpIn, TxEncoding InFmt, unsigned & Char)
{
   Char = 0;
   unsigned char c;

   // The magic numbers below are from the UTF specs.
   switch(InFmt)
   {
      case FMT_ANSI:
         {
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            Char = c;
         }
         break;

      case FMT_UTF8:
         {
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            if ((c & 0x80) == 0)
            {
               Char = c;
            }
            else if ((c & 0xE0) == 0xC0)
            {
               Char = (c & ~0xE0);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
            }
            else if ((c & 0xF0) == 0xE0)
            {
               Char = (c & ~0xF0);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
            }
            else if ((c & 0xF8) == 0xF0)
            {
               Char = (c & ~0xF8);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
            }
            else if ((c & 0xFC) == 0xF8)
            {
               Char = (c & ~0xFC);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
            }
            else if ((c & 0xFE) == 0xFC)
            {
               Char = (c & ~0xFE);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
               if (fread(&c, 1, 1, fpIn) != 1)
                  return false;
               Char = (Char << 6) + (c & 0x3F);
            }
            else
            {
               _ftprintf(stderr, "\nInvalid character sequence for UTF-8 at file offset %ld\n", ftell(fpIn));
               msg("Invalid character sequence for UTF-8");
               return false;
            }
         }
         break;

      case FMT_UTF16:
         {
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            Char = c;
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            Char = Char + static_cast<unsigned short>(c) * 256;
         }
         break;

      case FMT_UTF16BE:
         {
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            Char = c;
            if (fread(&c, 1, 1, fpIn) != 1)
               return false;
            Char = Char * 256 + static_cast<unsigned short>(c);
         }
         break;

      default:
         return false;
   }

   nChars++;
   return true;
}

//----------------------------------------------------------
// Read a line of text from the given file assuming the
// given encoding, placing the results into 'Line'.
// Returns true if successful, false if end of file.
//----------------------------------------------------------
static bool ReadLine(FILE *fpIn, TxEncoding InFmt, std::vector<unsigned> &Line)
{
   Line.clear();

   unsigned c;
   if (!ReadChar(fpIn, InFmt, c))
      return false;
   while (c != '\n')
   {
      Line.push_back(c);
      if (!ReadChar(fpIn, InFmt, c))
         return false;
   }
   Line.push_back(c);
   return true;
}

//----------------------------------------------------------
// Write character to the given output file, using the
// given encoding.
// Returns true if successful, false if write fails.
//----------------------------------------------------------
static bool WriteChar(
   FILE *fpOut,
   TxEncoding OutFmt,
   unsigned Char
   )
{
   // The magic numbers below are from the UTF specs.
   switch(OutFmt)
   {
      case FMT_ANSI:
         {
            unsigned char c = static_cast<unsigned char>(Char);
            if (fwrite(&c, 1, 1, fpOut) != 1)
               return false;
         }
         break;

      case FMT_UTF8:
         {
            if (Char <= 0x7F)
            {
               unsigned char c = static_cast<unsigned char>(Char);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
            else if (Char <= 0x7FF)
            {
               unsigned char c = 0xC0 | static_cast<unsigned char>(Char & 0x1F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 5;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
            else if (Char <= 0xFFFF)
            {
               unsigned char c = 0xE0 | static_cast<unsigned char>(Char & 0x0F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 4;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
            else if (Char <= 0x1FFFFF)
            {
               unsigned char c = 0xF0 | static_cast<unsigned char>(Char & 0x7);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 3;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
            else if (Char <= 0x3FFFFFF)
            {
               unsigned char c = 0xF8 | static_cast<unsigned char>(Char & 0x3);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 2;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
            else if (Char <= 0x7FFFFFFF)
            {
               unsigned char c = 0xFC | static_cast<unsigned char>(Char & 0x1);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 1;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
               Char >>= 6;
               c = 0x80 | static_cast<unsigned char>(Char & 0x3F);
               if (fwrite(&c, 1, 1, fpOut) != 1)
                  return false;
            }
         }
         break;

      case FMT_UTF16:
         {
            unsigned char c1 = static_cast<unsigned char>(Char & 0xFF);
            unsigned char c2 = static_cast<unsigned char>((Char >> 8) & 0xFF);
            if (fwrite(&c1, 1, 1, fpOut) != 1)
               return false;
            if (fwrite(&c2, 1, 1, fpOut) != 1)
               return false;
         }
         break;

      case FMT_UTF16BE:
         {
            unsigned char c1 = static_cast<unsigned char>((Char >> 8) & 0xFF);
            unsigned char c2 = static_cast<unsigned char>(Char & 0xFF);
            if (fwrite(&c1, 1, 1, fpOut) != 1)
               return false;
            if (fwrite(&c2, 1, 1, fpOut) != 1)
               return false;
         }
         break;

      default:
         return false;
   }

   return true;
}

//----------------------------------------------------------
// Write a line of text to the given output file, using the
// given encoding.
// Returns true if successful, false if write fails.
//----------------------------------------------------------
static bool WriteLine(
   FILE *fpOut,
   TxEncoding OutFmt,
   std::vector<unsigned> & Line
   )
{
   for (std::vector<unsigned>::iterator it = Line.begin(); it != Line.end(); ++it)
      if (!WriteChar(fpOut, OutFmt, *it))
         return false;
   return true;
}

//----------------------------------------------------------
// Write byte order marker for start of text file in the
// given encoding.
// Returns true if successful, false if write fails.
//----------------------------------------------------------
static bool WriteBOM(
   FILE *fpOut,
   TxEncoding OutFmt
   )
{
   // Write byte-order-mark bytes to start of file.
   // The magic numbers below are from the UTF specs.
   if (OutFmt == FMT_UTF8)
   {
      if (fwrite("\xEF\xBB\xBF", 1, 3, fpOut) != 3)
         return false;
   }
   else if (OutFmt == FMT_UTF16)
   {
      if (fwrite("\xFF\xFE", 1, 2, fpOut) != 2)
         return false;
   }
   else if (OutFmt == FMT_UTF16BE)
   {
      if (fwrite("\xFE\xFF", 1, 2, fpOut) != 2)
         return false;
   }
   // else:  other formats need no BOM bytes.

   return true;
}

//----------------------------------------------------------
// Attempts to identify the BOM at the start of the file.
// If successful, leaves the file pointer at the first
// character AFTER the BOM.
//
// Returns the text encoding indicated by the BOM if found,
// or FMT_UNKNOWN if not found.
//----------------------------------------------------------
TxEncoding CheckBOM(FILE *fpIn)
{
   TxEncoding InFmt = FMT_UNKNOWN;

   // Read first few bytes of file.
   unsigned char ch[32];
   memset(&ch[0], 0, 32);
   fseek(fpIn, 0, SEEK_SET);
   size_t bytes = fread(&ch, 1, _countof(ch), fpIn);
   fseek(fpIn, 0, SEEK_SET);
   if (bytes < 1)
   {
      msg("Empty input file");
      fclose(fpIn);
      return FMT_UNKNOWN;
   }

   // If input mode was not specified, attempt to determine format
   // from input data.  The magic numbers below are from the UTF specs.
   if (ch[0] == 0xFE && ch[1] == 0xFF)
   {
      InFmt = FMT_UTF16BE;
      fseek(fpIn, 2, SEEK_SET);
   }
   else if (ch[0] == 0xFF && ch[1] == 0xFE)
   {
      InFmt = FMT_UTF16;
      fseek(fpIn, 2, SEEK_SET);
   }
   else if (ch[0] == 0xEF && ch[1] == 0xBB && ch[2] == 0xBF)
   {
      InFmt = FMT_UTF8;
      fseek(fpIn, 3, SEEK_SET);
   }
   else if (bytes >= 16) // if we read at least 16 bytes...
   {
      // No BOM, but if the first chars are all ANSI/ASCII,
      // then assume the file format is ANSI.
      for (size_t i = 0; i < __min(bytes, _countof(ch)); i++)
      {
         if (ch[i] > 127)
         {
            return FMT_UNKNOWN;
         }
      }
      InFmt = FMT_ANSI;
   }
   return InFmt;
}

//----------------------------------------------------------
// main:
// Application entry point.
// Uses standard arguments and return values.
//----------------------------------------------------------
#ifdef _UNICODE
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
   // See if user needs command line help.
   if (argc < 2)
   {
      Usage();
      return EXIT_FAILURE;
   }

   // Program settings.
   std::string InFile;
   std::string OutFile;
   TxEncoding  InFmt  = FMT_AUTO;
   TxEncoding  OutFmt = FMT_ANSI;

   // Parse command line options.
   int nonopts = 0;
   for (int n = 1; n < argc; n++)
   {
      // If this argument is an option switch...
      if (argv[n][0] == '/' || argv[n][0] == '-')
      {
         if (OptionNameIs(argv[n], "INFORMAT") || OptionNameIs(argv[n], "I"))
         {
            // Specify the text encoding fo the input file.
            InFmt = TxEncodingFromName(OptionValue(argv[n]));
            if (InFmt == FMT_UNKNOWN)
            {
               msg("Unrecognized encoding option", argv[n]);
               return EXIT_FAILURE;
            }
         }
         else if (OptionNameIs(argv[n], "OUTFORMAT") || OptionNameIs(argv[n], "O"))
         {
            // Specify the text encoding fo the output file.
            OutFmt = TxEncodingFromName(OptionValue(argv[n]));
            if (OutFmt == FMT_UNKNOWN || OutFmt == FMT_AUTO)
            {
               msg("Unrecognized encoding option", argv[n]);
               return EXIT_FAILURE;
            }
         }
         else if (OptionNameIs(argv[n], "VERBOSE") || OptionNameIs(argv[n], "V"))
         {
            bVerbose = true;
         }
         else
         {
            msg("Unrecognized option", argv[n]);
            return EXIT_FAILURE;
         }
      }
      else
      {
         // This argument is a filename.
         nonopts++;
         if (nonopts == 1)
            InFile = argv[n];
         else if (nonopts == 2)
            OutFile = argv[n];
         else
         {
            msg("Too many arguments", argv[n]);
            return EXIT_SUCCESS;
         }
      }
   }

   // Make sure required argument(s) were given.
   if (InFile.size() < 1)
   {
      msg("No input file specified");
      return EXIT_FAILURE;
   }

   // Open the input file.
   FILE *fpIn = NULL;
   if (_tfopen_s(&fpIn, InFile.c_str(), "rb"))
   {
      msg("Failed opening input file", InFile.c_str());
      return EXIT_FAILURE;
   }

   // If input mode was not specified, attempt to determine format
   // from input data.
   TxEncoding BOMFmt = CheckBOM(fpIn);
   if (InFmt == FMT_AUTO)
   {
      if (BOMFmt == FMT_UNKNOWN)
      {
         msg("AUTO mode can't identify input format.  Please specify with /INFORMAT option.");
         fclose(fpIn);
         return EXIT_FAILURE;
      }
      InFmt = BOMFmt;
   }

   if (bVerbose)
   {
      // Determine file size.
      size_t OldPos = fseek(fpIn, 0, SEEK_CUR);
      size_t InLength = fseek(fpIn, 0, SEEK_END);
      fseek(fpIn, static_cast<long>(OldPos), SEEK_SET);
   
      // Read first few bytes of file.
      unsigned char ch[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
      fseek(fpIn, 0, SEEK_SET);
      size_t bytes = fread(&ch, 1, 8, fpIn);
      fseek(fpIn, static_cast<long>(OldPos), SEEK_SET);
      if (bytes < 1)
      {
         msg("Empty input file");
         fclose(fpIn);
         return EXIT_FAILURE;
      }

      _tprintf(_T("Input file:    \"%s\"\n"), InFile.c_str());
      _tprintf(_T("Input length:  %Iu bytes\n"), InLength);
      _tprintf(_T("Input format:  %s\n"), TxEncodingToName(InFmt));
      _tprintf(_T("Output file:   \"%s\"\n"), OutFile.size() > 0 ? OutFile.c_str() : _T("(stdout)"));
      _tprintf(_T("Output format: %s\n"), TxEncodingToName(OutFmt));
      _tprintf(_T("First %Iu bytes: "), bytes);
      for (size_t i = 0; i < bytes; i++)
         _tprintf(_T(" %02X"), ch[i]);
      _tprintf(_T("\n"));
   }

   // Open the output file, if any.
   FILE *fpOut = stdout;
   if (OutFile.size() > 0)
   {
      if (_tfopen_s(&fpOut, OutFile.c_str(), "wb"))
      {
         msg("Failed opening output file", OutFile.c_str());
         return EXIT_FAILURE;
      }
   }

   // Write byte order marker at start of file.
   if (!WriteBOM(fpOut, OutFmt))
   {
      fclose(fpIn);
      if (OutFile.size() > 0)
         fclose(fpOut);
      return EXIT_FAILURE;
   }

   // Process the input file.
   std::vector<unsigned> Line;
   nLines = nChars = 0;
   while (ReadLine(fpIn, InFmt, Line))
   {
      nLines++;
      if (!WriteLine(fpOut, OutFmt, Line))
      {
         msg("Failed writing output file");
         fclose(fpIn);
         if (OutFile.size() > 0)
            fclose(fpOut);
         return EXIT_FAILURE;
      }
   }

   if (bVerbose)
   {
      _ftprintf(stderr, "Lines Processed:  %Iu\n", nLines);
      _ftprintf(stderr, "Chars Processed:  %Iu\n", nChars);
   }

   // Clean up.
   fclose(fpIn);
   if (OutFile.size() > 0)
      fclose(fpOut);

   return EXIT_SUCCESS;
}

// End txu.cpp
