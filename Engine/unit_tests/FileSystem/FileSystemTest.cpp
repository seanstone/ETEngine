#include <EtFramework/stdafx.h>

#include <catch2/catch.hpp>

#include <thread>
#include <chrono>

#include <mainTesting.h>

#include <EtCore/FileSystem/Entry.h>
#include <EtCore/FileSystem/FileUtil.h>


using namespace et;


TEST_CASE( "mount", "[filesystem]" )
{
	std::string dirName = global::g_UnitTestDir + "FileSystem/TestDir/";
	core::Directory* pDir = new core::Directory( dirName, nullptr );

	REQUIRE( pDir->IsMounted() == false );

	bool mountResult = pDir->Mount( true );
	REQUIRE( mountResult == true );
	REQUIRE( pDir->IsMounted() == true );

	std::vector<core::Entry*> extChildren = pDir->GetChildrenByExt( "someFileExt" );
	bool found = false;
	for(auto cExt : extChildren)
	{
		if(cExt->GetType() == core::Entry::EntryType::ENTRY_FILE)
		{
			REQUIRE( cExt->GetNameOnly() == "test_file3.someFileExt" );
			found = true;
		}
	}
	REQUIRE( found == true );

	for(auto c : pDir->GetChildren())
	{
		if(c->GetType() == core::Entry::EntryType::ENTRY_DIRECTORY)
		{
			auto cDir = static_cast<core::Directory*>(c);
			REQUIRE( cDir->IsMounted() == true );
			REQUIRE( cDir->GetChildren().size() == 1 );
		}
	}

	pDir->Unmount();
	REQUIRE( pDir->IsMounted() == false );
	REQUIRE( pDir->GetChildren().size() == 0 );

	delete pDir;
	pDir = nullptr;
}

//this is a long test but a lot of the feature testing needs to work in a sequence
//if this test fails the test directory might need to be rebuilt or repaired manually
TEST_CASE( "copy file", "[filesystem]" )
{
	std::string expectedContent = "Hello I am a test file!\r\nwith 2 lines\r\n";
	auto expectedContentLines = core::FileUtil::ParseLines(expectedContent);

	std::string dirName = global::g_UnitTestDir + "FileSystem/TestDir/";
	core::Directory* pDir = new core::Directory( dirName, nullptr );
	bool mountResult = pDir->Mount( true );
	REQUIRE( mountResult == true );

	core::File* inputFile = nullptr;
	core::Directory* subDir = nullptr;
	for(auto c : pDir->GetChildren())
	{
		if(c->GetNameOnly() == "test_file.txt")
		{
			inputFile = static_cast<core::File*>(c);
		}
		if(c->GetType() == core::Entry::EntryType::ENTRY_DIRECTORY)
		{
			subDir = static_cast<core::Directory*>(c);
		}
	}
	REQUIRE_FALSE( inputFile == nullptr );
	REQUIRE_FALSE( subDir == nullptr );

	/* Create input file descriptor */
	core::File* outputFile = new core::File( "copy_file.txt", subDir );

	bool openResult = inputFile->Open( core::FILE_ACCESS_MODE::Read );
	REQUIRE( openResult == true );

	/* Create output file descriptor */
	core::FILE_ACCESS_FLAGS outFlags;
	outFlags.SetFlags( core::FILE_ACCESS_FLAGS::FLAGS::Create | core::FILE_ACCESS_FLAGS::FLAGS::Exists );
	bool openResult2 = outputFile->Open( core::FILE_ACCESS_MODE::Write, outFlags );
	REQUIRE( openResult2 == true );

	/* Copy process */
	std::string content = core::FileUtil::AsText(inputFile->Read());
	REQUIRE( core::FileUtil::ParseLines(content) == expectedContentLines );

	bool writeResult = outputFile->Write( core::FileUtil::FromText(content) );
	REQUIRE( writeResult == true );

	//for some reason need to close and reopen
	outputFile->Close();
	bool openResult3 = outputFile->Open( core::FILE_ACCESS_MODE::Read );
	REQUIRE( openResult3 == true );
	////Wait for file to be written (ewwww)
	//std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

	/* Copy process */
	std::string contentOut = core::FileUtil::AsText(outputFile->Read());
	REQUIRE( core::FileUtil::ParseLines(contentOut) == expectedContentLines );
	//outputFile->Close();

	/* Close file descriptors */
	pDir->Unmount();
	subDir = nullptr;
	inputFile = nullptr;
	outputFile = nullptr;

	// reopen and see if the file is there
	bool mountResult2 = pDir->Mount( true );
	REQUIRE( mountResult2 == true );
	for(auto c : pDir->GetChildren())
	{
		if(c->GetType() == core::Entry::EntryType::ENTRY_DIRECTORY)
		{
			subDir = static_cast<core::Directory*>(c);
			break;
		}
	}
	REQUIRE_FALSE( subDir == nullptr );
	for(auto c : subDir->GetChildren())
	{
		if(c->GetNameOnly() == "copy_file.txt")
		{
			outputFile = static_cast<core::File*>(c);
			break;
		}
	}
	REQUIRE_FALSE( outputFile == nullptr );
	bool openResult4 = outputFile->Open( core::FILE_ACCESS_MODE::Read );
	REQUIRE( openResult4 == true );
	std::string contentOut2 = core::FileUtil::AsText(outputFile->Read());
	REQUIRE( core::FileUtil::ParseLines(contentOut2) == expectedContentLines );

	outputFile->Close();

	//Deleting should handle closing the file
	bool deleteResult = outputFile->Delete();
	REQUIRE( deleteResult == true );
	deleteResult = false;

	//The file should be gone from the directory
	bool found = false;
	for(auto c : subDir->GetChildren())
	{
		if (c->GetNameOnly() == "copy_file.txt")
		{
			found = true;
		}
	}
	REQUIRE( found == false );

	//Should also hold true after remounting
	subDir->Unmount();
	bool mountResult3 = subDir->Mount( true );
	REQUIRE( mountResult3 == true );
	bool found2 = false;
	for(auto c : subDir->GetChildren())
	{
		if(c->GetNameOnly() == "copy_file.txt")
			found2 = true;
	}
	REQUIRE( found == false );

	//Deleting the parent directory should close all other stuff
	delete pDir;
	subDir = nullptr;
	outputFile = nullptr;
	pDir = nullptr;
}
