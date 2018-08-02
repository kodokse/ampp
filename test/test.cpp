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
  //etl::PdbFileManager fm;
  ////etl::PdbProvider pdbs;
  ////db.AddProvider(pdbs.Provide(L"C:\\Users\\Andreas Magnusson\\Source\\disum\\disum_driver\\out\\UM_USB\\x64"));
  //db.AddProvider(etl::PdbProvider(fm, L"C:\\Users\\Andreas Magnusson\\Source\\disum\\disum_driver\\out\\UM_USB\\x64"));
  //etl::LiveTraceEnumerator lte(db, L"Test1234");
  //lte.SetLogFunction([](const etl::TraceEventData &evt)
  //{
  //  std::wcout << evt.message << std::endl;
  //});
  etl::TxtfileEnumerator lte(db, L"C:\\Users\\Andreas Magnusson\\Work\\logs\\fail_login.log");
  lte.Start();
  //while (!_kbhit()) { Sleep(500); }
  //Sleep(5000);
  lte.Stop();
  for (size_t i = 0; i < __min(10, lte.GetItemCount()); ++i)
  {
    std::wcout << lte.GetItemValue(i, etl::TraceEventDataItem::TraceIndex) << L": " << lte.GetItemValue(i, etl::TraceEventDataItem::TimeStamp) << std::endl;
  }
  return 0;
}

