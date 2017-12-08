#pragma once
//{{{  includes
#include <map>
#include <string>
#include <utility>
//}}}

namespace htmlcxx {
  namespace HTML {
    class Node {
      public:
        Node() {}
        //Node(const Node &rhs); //uses default
        ~Node() {}

        // gets
        bool isTag() const { return this->mIsHtmlTag; }
        bool isComment() const { return this->mComment; }
        inline const std::string& text() const { return this->mText; }
        inline const std::string& closingText() const { return mClosingText; }
        inline unsigned int offset() const { return this->mOffset; }
        inline unsigned int length() const { return this->mLength; }
        inline const std::string& tagName() const { return this->mTagName; }

        // sets
        void isTag (bool is_html_tag){ this->mIsHtmlTag = is_html_tag; }
        void isComment (bool comment){ this->mComment = comment; }
        inline void text (const std::string& text) { this->mText = text; }
        inline void closingText (const std::string &text) { this->mClosingText = text; }
        inline void offset (unsigned int offset) { this->mOffset = offset; }
        inline void length (unsigned int length) { this->mLength = length; }
        inline void tagName (const std::string& tagname) { this->mTagName = tagname; }

        //{{{
        std::pair<bool, std::string> attribute (const std::string &attr) const {

          std::map<std::string, std::string>::const_iterator i = this->mAttributes.find(attr);
          if (i != this->mAttributes.end())
            return make_pair (true, i->second);
          else
            return make_pair (false, std::string());
          }
        //}}}

        operator std::string() const;
        std::ostream &operator << (std::ostream &stream) const;

        const std::map<std::string, std::string>& attributes() const { return this->mAttributes; }
        void parseAttributes();

        bool operator == (const Node &rhs) const;

      protected:
        std::string mText;
        std::string mClosingText;

        unsigned int mOffset;
        unsigned int mLength;

        std::string mTagName;
        std::map<std::string, std::string> mAttributes;

        bool mIsHtmlTag;
        bool mComment;
      };
    }
  }
