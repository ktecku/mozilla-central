/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 */

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "addurl.h"
#include "hash.h"
#include "html.h"
#include "http.h"
#include "main.h"
#include "mutex.h"
#include "url.h"
#include "utils.h"

typedef struct Arg
{
	URL	*url;
} Arg;

mutex_t mainMutex;

static char *limitURLs[] =
{
	"http://www.w3.org/TR/REC-CSS2/",
	NULL
};

static URL *lastURL = NULL;
static URL *urls = NULL;

void
reportContentType(void *a, unsigned char *contentType)
{
}

void
reportHTML(void *a, Input *input)
{
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
}

void
reportHTMLText(void *a, Input *input)
{
}

void
reportHTTP(void *a, Input *input)
{
}

void
reportHTTPBody(void *a, Input *input)
{
}

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
}

void
reportHTTPHeaderName(void *a, Input *input)
{
}

void
reportHTTPHeaderValue(void *a, Input *input)
{
}

void
reportStatus(void *a, char *message, char *file, int line)
{
}

void
reportTime(int task, struct timeval *theTime)
{
}

static void
addURLFunc(void *a, URL *url)
{
	lastURL->next = url;
	lastURL = url;
}

static void
grab(unsigned char *url, HTTP *http)
{
	char	*add;
	int	baseLen;
	FILE	*file;
	char	*p;
	char	*slash;
	char	*str;

	baseLen = strlen(limitURLs[0]);
	if (strncmp((char *) url, limitURLs[0], baseLen))
	{
		fprintf(stderr, "no match: %s vs %s\n", url, limitURLs[0]);
		return;
	}

	if (url[strlen((char *) url) - 1] == '/')
	{
		add = "index.html";
	}
	else
	{
		add = "";
	}

	str = calloc(strlen((char *) url + baseLen) + strlen(add) + 1, 1);
	if (!str)
	{
		fprintf(stderr, "cannot calloc string\n");
		exit(0);
	}
	strcpy(str, (char *) url + baseLen);
	p = strchr(str, '#');
	if (p)
	{
		*p = 0;
	}
	strcat(str, add);
	p = str;
	while (1)
	{
		slash = strchr(p, '/');
		if (!slash)
		{
			break;
		}
		*slash = 0;
		if (mkdir(str, 0777))
		{
			if (errno != EEXIST)
			{
				perror("mkdir");
			}
		}
		*slash = '/';
		p = slash + 1;
	}
	file = fopen(str, "w");
	if (!file)
	{
		fprintf(stderr, "cannot open file %s for writing\n", str);
		exit(0);
	}
	if (fwrite(http->body, 1, http->bodyLen, file) != http->bodyLen)
	{
		fprintf(stderr, "did not write %ld bytes\n", http->bodyLen);
		exit(0);
	}
	fclose(file);
	free(str);
}

int
main(int argc, char *argv[])
{
	Arg	arg;
	HTTP	*http;
	char	*prog;
	URL	*url;

	MUTEX_INIT();

	prog = strrchr(argv[0], '/');
	if (prog)
	{
		prog++;
	}
	else
	{
		prog = argv[0];
	}

	switch (argc)
	{
	case 1:
		break;
	case 2:
		limitURLs[0] = argv[1];
		break;
	default:
		fprintf(stderr, "usage: %s [ http://www.foo.com/bar/ ]\n",
			prog);
		return 1;
	}

	addURLInit(addURLFunc, limitURLs, NULL);

	url = urlParse((unsigned char *) limitURLs[0]);
	urls = url;
	lastURL = url;
	while (url)
	{
		arg.url = url;
		http = httpProcess(&arg, url, NULL);
		if (http)
		{
			switch (http->status)
			{
			case 200: /* OK */
				grab(url->url, http);
				break;
			case 302: /* Moved Temporarily */
				break;
			case 403: /* Forbidden */
				break;
			case 404: /* Not Found */
				break;
			default:
				printf("status %d\n", http->status);
				break;
			}
			httpFree(http);
		}
		else
		{
			printf("httpProcess failed: %s\n", url->url);
		}
		url = url->next;
	}

	return 0;
}
