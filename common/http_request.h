
#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include <vector>
#include <string>

namespace utils {

class HttpRequest
{
public:
    /*************************************************
      GET/POST http page from @url, save @page
      @return 0: success  <0: fail
    *************************************************/
    static int Get(const std::string& url,
                    std::string& page,
                    std::string* errmsg=NULL,
                    unsigned int timeout_s=10);
    static int Get(const std::string& url,
                    const std::vector<std::string> headers,
                    std::string& page,
                    std::string* errmsg,
                    unsigned int timeout_s=10);

    static int Post(const std::string& url,
                    const std::string& form_data,
                    std::string& page,
                    std::string* errmsg=NULL,
                    unsigned int timeout_s=10);
    static int Post(const std::string& url,
                    const std::vector<std::string>& headers,
                    const std::string& form_data,
                    std::string& page,
                    std::string* errmsg=NULL,
                    unsigned int timeout_s=10);
    static int Delete(const std::string& url,
                    const std::vector<std::string>& headers,
                    std::string& form_data,
                    std::string& page,
                    std::string* errmsg=NULL,
                    unsigned int timeout_s=10);

};

}

#endif


