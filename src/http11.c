/*
	http11
	Written by Mike Perron (2012-2013)

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	Functions that implement the basics of HTTP/1.1. This includes GET and
	POST. My simple web server will pass FILE streams and URIs to these
	functions, and they will deal with them appropriately.

	GET and POST will maybe be a wrapper to a single other function, which is
	both GET and POST depending on the flag (since the two requests are
	fundamentally similar).
*/
#include "http11.h"
enum conn_mode { CLOSE, KEEP_ALIVE, UNSET };

char *get_mime_type(char *filename){
	char *str, *s, *a, *fname=NULL;
	FILE *mime;
	size_t n;
	char c=0;

	// Find the file part of the name.
	a=filename+strlen(filename);
	while(a!=filename){
		if(*a=='.'){
			a++;
			break;
		}
		a--;
	}
	if(a==filename)
		a=&c;

	// Get a handle to the mime list.
	if(!(s=getenv("conf_dir")))
		return NULL;
	str=calloc(n=(strlen(s)+256), sizeof(char));
	sprintf(str, "%s/mime", s);

	// Rock 'n' Roll.
	if(!(mime=fopen(str, "r")))
		return NULL;
	while(getline(&str, &n, mime)!=-1){
		if(*str=='#')
			continue;

		for(s=str;*s;s++)
			if(*s==' '||*s=='\t')break;
		*(s++)=0;

		if(!strcmp(a, str)){
			for(a=s;*s==' '||*s=='\t';s++);
			for(a=s;*s;s++)if(*s=='\r'||*s=='\n')
				break;
			*s=0;
			fname=calloc(1+strlen(a), sizeof(char));
			strcpy(fname, a);
			break;
		}
	}

	if(!fname){
		fname=calloc(32, sizeof(char));
		strcpy(fname, "application/octet-stream");
	}

	free(str);
	fclose(mime);
	return fname;
}

// GET and POST is handled here.
void http_request(FILE *stream, char *uri, int method){
	char **a, *mime_type, *index, *cgi_content=NULL;
	char *s, *str, *b, **cgi_headers=NULL;
	char *status;

	int c, cgi_headers_count=0;
	long int count=0;
	size_t n;

	FILE *post_data_file=NULL;
	char *post_data_fname=NULL;
	char *post_raw_data=NULL;
	size_t post_length=0;

	FILE *content, *cgi_pipe, *conf;
	struct stat sbuf;

	enum conn_mode conn_mode=CLOSE;

	// Whether we will persist the TCP connection.
	if((s=getenv("CONNECTION_MODE")) && !strcasecmp(s, "keep-alive"))
		conn_mode=KEEP_ALIVE;

	// Gets set if we're going to do an auto directory listing.
	char listing_mode=0;

	// Clear the buffer of POST data
	if(method==POST){
		if(s=getenv("CONTENT_LENGTH"))
			post_length=atoi(s);
		if(post_length>0){
			post_raw_data=calloc(post_length+1, sizeof(char));
			*(post_raw_data+post_length)=0;
			fread(post_raw_data, sizeof(char), post_length, stream);
		}
	}

	if(strstr(uri, "/../"))
		return http_default_error(stream, 401, "Permission Denied.");

	// Stat the file.
	if(stat(uri, &sbuf)==-1)
		return http_default_error(stream, 404, "File Not Found.");
	else {
		if(S_ISDIR(sbuf.st_mode)){
			str=calloc(strlen(uri)+256, sizeof(char));

			// Redirect if the trailing slash is missing.
			if(*(uri+strlen(uri)-1)!='/'){
				sprintf(str, "%s/", uri+strlen(getenv("web_root")));
				return http_redirect(stream, 301, str);
			}

			sprintf(str, "%s/serv", (s=getenv("conf_dir"))?s:".");

			// Check conf/serv for a list of default documents.
			if(s=get_conf_line(str, "default_documents")){
				unquote_str(s);

				// Parse and attempt to discover each of the files.
				while(*s){
					for(b=s;*b;b++)
						if(*b==';')
							break;
					c=*b;*b=0;

					sprintf(str, "%s/%s", uri, s);
					if(stat(str, &sbuf)<0)
						s=(c)?b+1:b;
					else break;
				}

			// No config for default docs, just try the Apache standard.
			} else sprintf(str, "%s/index.html", uri);

			// If the path isn't canonical redirect the user.
			if((s=getenv("kws_pot_err")) && strstr(s, "dirnotdir")){
				if(s=strstr(str, getenv("web_root")))
					s+=strlen(getenv("web_root"));
				else s=str;
				return http_redirect(stream, 301, s);
			}
			unsetenv("kws_pot_err");

			// Directory listing mode.
			if(stat(str, &sbuf)<0)
				listing_mode=1;

			uri=realloc(uri, (strlen(str)+1)*sizeof(char));
			strcpy(uri, str);
			free(str);
		}
		
		// Check for exec permissions or Kraknet.
		if(listing_mode){
			mime_type=calloc(32, sizeof(char));
			strcpy(mime_type, "text/html; charset=UTF-8");
		} else mime_type=get_mime_type(uri);
		s=NULL;
		if(listing_mode || sbuf.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH) || (s=strstr(mime_type, "text/html"))){
			setenv("SCRIPT_NAME", uri+strlen(getenv("web_root")), 1);
			/**********************************************
			    Output mode is CGI. Running the script.
			**********************************************/
			str=calloc(n=256, sizeof(char));
			if(method==POST){
				// TODO: Add a default protection of /tmp if $tmp_ws has no data.
				post_data_fname=calloc(strlen(getenv("tmp_ws"))+32, sizeof(char));
				sprintf(post_data_fname, "%s/%s", getenv("tmp_ws"), post_time(KRAKNET_POST, 1));

				if(post_data_file=fopen(post_data_fname, "w")){
					fwrite(post_raw_data, sizeof(char), post_length, post_data_file);
					fclose(post_data_file);
				} else {
					free(post_data_fname);
					post_data_fname=NULL;
				}
			}

			// TODO: If POST, pipe the temp file into this command.
			if(post_data_fname)
				sprintf(str, "%s\"%s\" < %s %s", s?"kraknet ":"", listing_mode?"list":uri, post_data_fname, (getenv("log_root"))?"2>>$log_root/cgi.log":"");
			else sprintf(str, "%s\"%s\" %s", s?"kraknet ":"", listing_mode?"list":uri, (getenv("log_root"))?"2>>$log_root/cgi.log":"");

			if(cgi_pipe=popen(str, "r")){
				// Read CGI headers from script.
				while(getline(&str, &n, cgi_pipe)!=-1){
					if(str==strstr(str, "\r\n"))break;
					if(str==strstr(str, "\n"))break;
					sanitize_str(str);

					cgi_headers=realloc(cgi_headers, (++cgi_headers_count+1)*sizeof(char*));
					*(cgi_headers+cgi_headers_count-1)=calloc(n, sizeof(char));

					strcpy(*(cgi_headers+cgi_headers_count-1), str);
				}	*(cgi_headers+cgi_headers_count)=NULL;

				// Read the output of the CGI Script.
				do{	if(feof(cgi_pipe))
						break;
					cgi_content=realloc(cgi_content, count+256);
					n=fread(cgi_content+count, sizeof(char), 256, cgi_pipe);
				}	while(count+=n);
				pclose(cgi_pipe);

				//Clean up temp data.
				if(post_data_fname)
					unlink(post_data_fname);

				// TODO: Parse CGI header and correct it.
				status=calloc(256, sizeof(char));
				strcpy(status, "200 OK");
				for(a=cgi_headers;*a;a++){
					if(!strncasecmp(*a, "Status: ", 8)){
						strcpy(status, *a+8);
						if(!*status)
							return http_default_error(stream, 500, "Status was ill-defined.");
						**a=0;
					}
					else if(!strncasecmp(*a, "Connection: ", 12))
						conn_mode=UNSET;
				}
				fprintf(stream, "%s %s\r\n", getenv("SERVER_PROTOCOL"), status);
				fputs("Date: ", stream);
				http_date(stream, 0);
				fputs("\r\n", stream);
				fprintf(stream, "Server: %s\r\n", KWS_SERVER_NAME);
				if(conn_mode!=UNSET)
					fprintf(stream, "Connection: %s\r\n", (conn_mode==KEEP_ALIVE)?"keep-alive":"close");

				// Print \r\n terminated header lines.
				for(a=cgi_headers;*a;a++)
					if(**a)
						fprintf(stream, "%s\r\n", *a);
				fprintf(stream, "Content-length: %ld\r\n", count);
				fputs("\r\n", stream);

				// CGI output
				if(method!=HEAD)
					fwrite(cgi_content, sizeof(char), count, stream);

				free(cgi_content);
				free(status);
			} else return http_default_error(stream, 501, "CGI Error.");
			free(str);
		} else {
			/**********************************************
			    Output mode is NOT CGI. Dumping a file.
			**********************************************/
			// Header output
			if(!mime_type)
				return http_default_error(stream, 500, "MIME Type Not Found.");
			else {
				fprintf(stream, "HTTP/1.1 200 OK\r\n");

				fputs("Date: ", stream);
				http_date(stream, 0);
				fputs("\r\n", stream);

				fprintf(stream,
					"Server: "KWS_SERVER_NAME"\r\n"
					"Connection: %s\r\n"
					"Content-Type: %s\r\n"
					"Content-Length: %ld\r\n\r\n",
					(conn_mode==KEEP_ALIVE)?"keep-alive":"close",
					mime_type, sbuf.st_size
				);

				if(method!=HEAD){
					// Dump file contents.
					content=fopen(uri, "r");
					while(1){
						c=getc(content);
						if(feof(content))
							break;
						fputc(c, stream);
					}	fclose(content);
				}
				free(mime_type);
			}
		}
	}
	free(uri);
}

void http_default_error(FILE *stream, int code, const char *optional_msg){
	enum conn_mode conn_mode=CLOSE;
	char *s;

	// Whether we will persist the TCP connection.
	if((s=getenv("CONNECTION_MODE")) && !strcasecmp(s, "keep-alive"))
		conn_mode=KEEP_ALIVE;

	/*	If the optional_msg is NULL, use a default error code message. */	
	fprintf(stream,
		"HTTP/1.1 %d %s\r\n",
		code,
		(optional_msg)?optional_msg:"OK"
	);

	fputs("Date: ", stream);
	http_date(stream, 0);
	fputs("\r\n", stream);

	fprintf(stream,
		"Server: "KWS_SERVER_NAME"\r\n"
		"Connection: %s\r\n"
		"Content-type: text/html; charset=UTF-8\r\n"
		"Content-length: 5\r\n\r\n"
		"%3d\r\n",
		(conn_mode==KEEP_ALIVE)?"keep-alive":"close",
		code
	);
}


/*	GMT time stamp to make the browser feel all warm and fuzzy. */
void http_date(FILE *stream, int offset_sec){
	char *str=calloc(256,sizeof(char));

	time_t now=time(0);
	strftime(str, 256, "%a, %d %b %Y %H:%M:%S %Z", gmtime(&now));
	fputs(str, stream);

	free(str);
}

void http_redirect(FILE *stream, int code, const char *uri_moved){
	enum conn_mode conn_mode=CLOSE;
	char *s;

	// Whether we will persist the TCP connection.
	if((s=getenv("CONNECTION_MODE")) && !strcasecmp(s, "keep-alive"))
		conn_mode=KEEP_ALIVE;

	fprintf(stream,
		"HTTP/1.1 %d Moved Permanently\r\n",
		code?:301
	);

	fputs("Date: ", stream);
	http_date(stream, 0);
	fputs("\r\n", stream);

	fprintf(stream,
		"Server: "KWS_SERVER_NAME"\r\n"
		"Connection: %s\r\n"
		"Location: %s\r\n"
		"Content-Length: 0\r\n\r\n",
		(conn_mode==KEEP_ALIVE)?"keep-alive":"close",
		uri_moved
	);
}
