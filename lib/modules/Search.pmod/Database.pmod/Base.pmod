// This file is part of Roxen Search
// Copyright � 2001 Roxen IS. All rights reserved.
//
// $Id: Base.pmod,v 1.10 2001/07/31 15:29:47 js Exp $

//! Base class for Roxen Search database storage abstraction implementations.


//! @decl void create(string db_url);
//! Initialize the database object.
//! @param path
//!   The URL that identifies the underlying database storage

//! Retrieve and possibly creates the URI id corresponding to the given URI.
//! @param uri
//!   The URI to be retrieved or created.
//! @param do_not_create
//!   If non-zero, do not create the URI.
//! @returns
//!   The non-zero numeric identifier if @[uri] exists, or 0 otherwise.
int get_uri_id(string uri,  void|int do_not_create);

//! Retrieve and possibly creates the document id corresponding to the given URI
//! and language code.
//! @param uri
//!   The URI to be retrieved or created.
//! @param language
//!   A three letter ISO-639-2 language code, or 0 if the document is language neutral.
//! @param do_not_create
//!   If non-zero, do not create the document.
//! @returns
//!   The non-zero numeric identifier if the document identified by @[uri]
//!   and @[language_code] exists, or 0 otherwise.
int get_document_id(string uri, void|string language, void|int do_not_create);

//! Retrieve the URI and language code associated with @[doc_id].
//! @returns
//!   @mapping
//!     @member string "uri"
//!       The URI of the document.
//!     @member void|string "language""
//!       The ISO-639-2 language code of the document, or 0 if not set.
//!   @endmapping
mapping get_uri_and_language(int doc_id);

//! Index words into the database. The data may be buffered until the next @[sync] call.
//! @param uri
//!   The URI of the resource being indexed.
//! @param language
//!   A three letter ISO-639-2 language code, or 0 if the document is language neutral.
//! @param field
//!   The field name for the words being indexed.
//! @param words
//!   The words being indexed. Possibly in wide-string format. (Not UTF8 encoded.)
void insert_words(Standards.URI|string uri,
		  void|string language,
		  string field,
		  array(string) words);

//! Set a metadata collection for a document.
//! @param uri
//!   The URI of the resource being indexed.
//! @param language
//!   A three letter ISO-639-2 language code, or 0 if the document is language neutral.
//! @param metadata
//!   A collection of metadata strings to be set for a document. The strings may be wide.
//!   The "body" metadata may be cut off at 64K.
void set_metadata(Standards.URI|string uri,
		  void|string language,
		  mapping(string:string) metadata);

//! Retrieve a metadata collection for a document.
//! @param uri
//!   The URI of the resource being indexed.
//! @param language
//!   A three letter ISO-639-2 language code, or 0 if the document is language neutral.
//! @param wanted_fields
//!   An array containing the wanted metadata field names, or 0.
//! @returns
//!   The metadata fields in @[wanted_fields] or all existing fields if @[wanted_fields] is 0.
mapping(string:string) get_metadata(int|Standards.URI|string uri,
				    void|string language,
				    void|array(string) wanted_fields);

//! Remove a document from the database. 
//! @param uri
//!   The URI of the resource being indexed.
//! @param language
//!   A three letter ISO-639-2 language code. If zero, delete all existing language forks
//!   with the URI of @[uri].
void remove_document(string|Standards.URI uri, void|string language);

//! Writes the data stored in temporary buffers to permanent storage.
//! Calls the function set by @[set_sync_callback]] when done.
void sync();

//! Sets a function to be called when @[sync] has been completed.
void set_sync_callback(function f);

//! Retrieves a blob from the database.
//! @param word
//!   The wanted word. Possibly in wide-string format. (Not UTF-8 encoded.)
//! @returns
//!   The blob requested, or 0 if there's no more blobs.
// FIXME
string get_blob(string word, int num);

//! Retrieves a list of deleted document ids
//! @returns
//!   An array containing deleted document ids.
array(int) get_deleted_documents();
  
//! Allocate a field id.
//! @param field
//!   The (possibly wide string) field name wanted.
//! @returns
//!   An allocated numeric id, or -1 if the allocation failed.
int allocate_field_id(string field);

//! Retrieve and possibly creates the numeric id of a field
//! @param field
//!   The (possibly wide string) field name wanted.
//! @param do_not_create
//!   If non-zero, do not allocate a field id for this field
//! @returns
//!   An allocated numeric id, or -1 if it did not exist, or allocation failed.
int get_field_id(string field, void|int do_not_create);

//! Remove a field from the database. Also removes all stored metadata with this field, but
//! not all indexed words using this field id.
//! @param field
//!   The (possibly wide string) field name to be removed.
void remove_field(string field);

//! Remove a field from the database. Also removes all stored metadata with this field, but
//! not all indexed words using this field id.
//! @returns
//!   A mapping with the fields in the index part, and the corresponding numeric field id as values.
mapping(string:int) list_fields();
