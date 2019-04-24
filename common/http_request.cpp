
#include <curl/curl.h>
#include "http_request.h"

using namespace std;

namespace utils {

static char errorBuffer[CURL_ERROR_SIZE];
static int writer(char *, size_t, size_t, string *);
static bool init(CURL *&curl, const char *url, string *p_buffer, int timeout_s=5, std::string* errmsg=NULL);
static bool curl_init = false;

int HttpRequest::Get(const std::string& url, std::string& page, std::string* errmsg, unsigned int timeout_s/*=5*/)
{
    int res = -1;
    CURL *curl = NULL;
    do {
        if (!init(curl, url.c_str(), &page, timeout_s, errmsg)) {
            break;
        }
    
        CURLcode code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            if(errmsg)
                *errmsg = "Get curl_easy_perform " + std::string(errorBuffer);
            break;
        }
        res = 0;    // done
    } while(false);
    if(curl) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }
    return res;
}

int HttpRequest::Get(const std::string& url, const std::vector<std::string> headers, std::string& page, std::string* errmsg, unsigned int timeout_s/*=5*/)
{
    int res = -1;
    CURL *curl= NULL;
    struct curl_slist *chunk = NULL;
    do {
        if (!init(curl, url.c_str(), &page, timeout_s, errmsg)) {
            break;
        }
        for(size_t i=0; i<headers.size(); i++) {
            const std::string& header = headers[i];
            chunk = curl_slist_append(chunk, header.c_str());
        }
        if(chunk)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    
        CURLcode code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            if(errmsg)
                *errmsg = "Get curl_easy_perform " + std::string(errorBuffer);
            break;
        }
        res = 0;    // done
    } while(false);

    if(curl) {
        curl_easy_cleanup(curl);
    }
    /* free the custom headers */ 
    if(chunk) {
        curl_slist_free_all(chunk);
    }
    return res;
}

static bool init(CURL *&curl, const char *url, string *p_buffer, int timeout_s/*=5*/, std::string* errmsg/*=NULL*/)
{
    bool res = false;
    do {
        if(!curl_init){
            curl_init = true;
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }

        CURLcode code;
        curl = curl_easy_init();
        if (curl == NULL)
        {
            if(errmsg)
                *errmsg = "init curl_easy_init Failed to create CURL curlection";
            break;
        }
        code=curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_NOSIGNAL failed";
            break;
        }
        code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_ERRORBUFFER " + std::string(errorBuffer);
            break;
        }
        code = curl_easy_setopt(curl, CURLOPT_URL, url);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_URL " + std::string(errorBuffer);
            break;
        }
        code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_FOLLOWLOCATION " + std::string(errorBuffer);
            break;
        }
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_WRITEFUNCTION " + std::string(errorBuffer);
            break;
        }
        code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, p_buffer);
        if (code != CURLE_OK)
        {
            if(errmsg)
                *errmsg = "init curl_easy_setopt CURLOPT_WRITEDATA " + std::string(errorBuffer);
            break;
        }
        if(timeout_s > 0)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

        res = true;
    } while(0);

    return res;
}

static int writer(char *data, size_t size, size_t nmemb, string *writerData)
{
    unsigned long sizes = size * nmemb;
    if (writerData == NULL) return 0;
    writerData->append(data, sizes);
    return sizes;
}

int HttpRequest::Post(const std::string& url, const std::string& form_data, std::string& page, std::string* errmsg, unsigned int timeout_s) {

    int ret = -1;

    //static const char *postthis="Field=1&Field=2&Field=3";
    if(!curl_init){
        curl_init = true;
        curl_global_init(CURL_GLOBAL_ALL);
    }
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());

        /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by
           itself */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form_data.size());

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            if(errmsg)
                *errmsg = "Post curl_easy_perform : " + std::string(curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);

        /* we're done with libcurl, so clean it up */

        ret = (res==CURLE_OK?0:-1);
        //return res==CURLE_OK?0:-1;
    } else {
        if(errmsg)
            *errmsg = "Post curl_easy_init failed";
    }
    return ret;
}

int HttpRequest::Post(const std::string& url, const std::vector<std::string>& headers, const std::string& form_data, std::string& page, std::string* errmsg, unsigned int timeout_s) {
    int ret = -1;

    if(!curl_init){
        curl_init = true;
        curl_global_init(CURL_GLOBAL_ALL);
    }
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);

        //std::string stoken = std::string("Cookie: PHPSESSID=") + token;
        struct curl_slist *chunk = NULL;
        for(size_t i=0; i<headers.size(); i++) {
            const std::string& header = headers[i];
            chunk = curl_slist_append(chunk, header.c_str());
        }
        if(chunk)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);


        if(!form_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());
            /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by itself */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form_data.size());
        }

        if(timeout_s > 0)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            if(errmsg)
                //*errmsg = "Post curl_easy_perform " + std::string(curl_easy_strerror(res));
                *errmsg = "Post curl_easy_perform : " + std::string(curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);

        /* free the custom headers */ 
        if(chunk)
            curl_slist_free_all(chunk);

        /* we're done with libcurl, so clean it up */

        //return res==CURLE_OK?0:-1;
        ret = (res==CURLE_OK?0:-1);
    } else {
        if(errmsg)
            *errmsg = "Post curl_easy_init failed";
    }

    return ret;
}

int HttpRequest::Delete(const std::string& url, const std::vector<std::string>& headers, std::string& form_data, std::string& page, std::string* errmsg, unsigned int timeout_s) {
    int ret = -1;

    if(!curl_init){
        curl_init = true;
        curl_global_init(CURL_GLOBAL_ALL);
    }
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);

        //std::string stoken = std::string("Cookie: PHPSESSID=") + token;
        struct curl_slist *chunk = NULL;
        for(size_t i=0; i<headers.size(); i++) {
            const std::string& header = headers[i];
            chunk = curl_slist_append(chunk, header.c_str());
        }
        if(chunk)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        if(!form_data.empty()) {
            /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by itself */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form_data.size());
        }

        if(timeout_s > 0)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);

        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            if(errmsg)
                //*errmsg = "Post curl_easy_perform " + std::string(curl_easy_strerror(res));
                *errmsg = "Post curl_easy_perform : " + std::string(curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);

        /* free the custom headers */ 
        if(chunk)
            curl_slist_free_all(chunk);

        /* we're done with libcurl, so clean it up */

        //return res==CURLE_OK?0:-1;
        ret = (res==CURLE_OK?0:-1);
    } else {
        if(errmsg)
            *errmsg = "Post curl_easy_init failed";
    }

    return ret;
}

}
