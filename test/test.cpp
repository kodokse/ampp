// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <ampp/ampp.h>

#include <ampp/etl/trace_enumerator.h>
#include <ampp/etl/pdb_provider.h>
#include <ampp/etl/pdb_file.h>

#include <conio.h>

int _tmain(int argc, _TCHAR* argv[])
{
  etl::FormatDatabase db;
  etl::PdbFileManager fm;
  //etl::PdbProvider pdbs;
  //db.AddProvider(pdbs.Provide(L"C:\\Users\\Andreas Magnusson\\Source\\disum\\disum_driver\\out\\UM_USB\\x64"));
  db.AddProvider(etl::PdbProvider(fm, L"C:\\Users\\Andreas Magnusson\\Source\\disum\\disum_driver\\out\\UM_USB\\x64"));
  etl::LiveTraceEnumerator lte(db, L"Test1234");
  lte.SetLogFunction([](const etl::TraceEventData &evt)
  {
    std::wcout << evt.message << std::endl;
  });
  lte.Start();
  while (!_kbhit()) { Sleep(500); }
  //Sleep(5000);
  lte.Stop();
  return 0;
}

