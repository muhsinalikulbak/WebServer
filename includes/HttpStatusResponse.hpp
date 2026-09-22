#ifndef HTTPSTATUSRESPONSE_HPP
#define HTTPSTATUSRESPONSE_HPP

#include "HttpResponse.hpp"

struct LocationConfig;

// HttpStatusResponse: generic HTTP status cevabı üretir (hata sayfası DEĞİL).
// build() config'teki error_page'e bakmaz; ErrorResponse::build hata modülü iken
// bu modül "kodu ve opsiyonel Location header'ını zaten biliyorum, sadece HTML
// gövdesiyle sarmala" diyen çağıranlara hizmet eder (200/201 başarı cevapları dahil).
namespace HttpStatusResponse
{
    // Shared HTML gövdesini üretir: "<title>KOD</title> + <h1>KOD AÇIKLAMA</h1>".
    std::string html(int statusCode);

    // Ortak status cevabı üretmek için tek noktadan body/header kurar.
    // Redirect gibi durumlarda aynı HTML şablonunu tekrar tekrar yazmamak amaçlanır.
    // Location header yalnızca gerçekten gerekli olduğunda eklenir.
    HttpResponse build(int statusCode, const std::string& locationHeader = "");

    // Location return kuralını HTTP redirect cevabına çevirir.
    HttpResponse redirect(const LocationConfig& location);
}

#endif