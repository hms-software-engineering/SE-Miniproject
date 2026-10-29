#include "reports/report_service.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace hms::reports {
namespace {

std::string csv_field(const std::string& value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    escaped += '"';
    return escaped;
}

std::string pdf_escape(const std::string& value)
{
    std::string escaped;
    for (const char character : value) {
        if (character == '(' || character == ')' || character == '\\') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

std::string pdf_number(std::size_t value)
{
    return std::to_string(value);
}

}  // namespace

std::string to_csv(const std::vector<DailyReportRow>& rows)
{
    std::ostringstream csv;
    csv << "date,appointment_count,confirmed_count,cancelled_count,"
           "completed_count,no_show_count,revenue\n";
    for (const auto& row : rows) {
        csv << csv_field(row.date) << ',' << row.appointment_count << ','
            << row.confirmed_count << ',' << row.cancelled_count << ','
            << row.completed_count << ',' << row.no_show_count << ','
            << csv_field(row.revenue) << '\n';
    }
    return csv.str();
}

std::string to_pdf(const std::vector<DailyReportRow>& rows,
                   const std::string& start_date,
                   const std::string& end_date)
{
    constexpr std::size_t rows_per_page = 42;
    const std::size_t page_count =
        rows.empty() ? 1 : (rows.size() + rows_per_page - 1) / rows_per_page;
    const std::size_t object_count = 3 + page_count * 2;
    std::vector<std::string> objects(object_count + 1);

    objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
    objects[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>";

    std::ostringstream kids;
    kids << "<< /Type /Pages /Kids [";
    for (std::size_t page = 0; page < page_count; ++page) {
        const std::size_t page_id = 4 + page * 2;
        const std::size_t content_id = page_id + 1;
        kids << page_id << " 0 R ";

        std::ostringstream content;
        content << "BT\n/F1 9 Tf\n45 750 Td\n";
        const auto line = [&content](const std::string& text) {
            content << '(' << pdf_escape(text) << ") Tj\n0 -14 Td\n";
        };
        line("Daily appointment and revenue report");
        line("Date range: " + start_date + " to " + end_date);
        line("Date       Total Confirmed Cancelled Completed No-show Revenue");
        line("----------------------------------------------------------------");

        const std::size_t first_row = page * rows_per_page;
        const std::size_t last_row =
            std::min(rows.size(), first_row + rows_per_page);
        for (std::size_t index = first_row; index < last_row; ++index) {
            const auto& row = rows[index];
            std::ostringstream text;
            text << row.date << ' ' << std::setw(5) << row.appointment_count
                 << ' ' << std::setw(9) << row.confirmed_count << ' '
                 << std::setw(9) << row.cancelled_count << ' '
                 << std::setw(9) << row.completed_count << ' '
                 << std::setw(7) << row.no_show_count << ' '
                 << row.revenue;
            line(text.str());
        }
        content << "ET\n";

        const std::string stream = content.str();
        objects[page_id] =
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            "/Resources << /Font << /F1 3 0 R >> >> /Contents " +
            pdf_number(content_id) + " 0 R >>";
        objects[content_id] =
            "<< /Length " + pdf_number(stream.size()) +
            " >>\nstream\n" + stream + "endstream";
    }
    kids << "] /Count " << page_count << " >>";
    objects[2] = kids.str();

    std::string pdf = "%PDF-1.4\n";
    std::vector<std::size_t> offsets(object_count + 1, 0);
    for (std::size_t id = 1; id <= object_count; ++id) {
        offsets[id] = pdf.size();
        pdf += pdf_number(id) + " 0 obj\n" + objects[id] + "\nendobj\n";
    }

    const std::size_t xref_offset = pdf.size();
    pdf += "xref\n0 " + pdf_number(object_count + 1) +
           "\n0000000000 65535 f \n";
    for (std::size_t id = 1; id <= object_count; ++id) {
        std::ostringstream offset;
        offset << std::setw(10) << std::setfill('0') << offsets[id];
        pdf += offset.str() + " 00000 n \n";
    }
    pdf += "trailer\n<< /Size " + pdf_number(object_count + 1) +
           " /Root 1 0 R >>\nstartxref\n" + pdf_number(xref_offset) +
           "\n%%EOF\n";
    return pdf;
}

}  // namespace hms::reports
