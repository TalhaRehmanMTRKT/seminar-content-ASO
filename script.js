// Load CSV file and initialize DataTable
$(document).ready(function () {
  const csvFilePath = "code/results.csv";

  $.ajax({
    url: csvFilePath,
    dataType: "text",
  })
    .done(function (data) {
      // Parse CSV
      const parsed = Papa.parse(data, { header: true });
      if (parsed.errors.length) {
        console.error("CSV parsing errors:", parsed.errors);
        return;
      }

      // Prepare columns for DataTables
      const columns = Object.keys(parsed.data[0]).map((key) => ({
        title: key,
        data: key,
      }));

      // Initialize DataTable
      $("#csvTable").DataTable({
        data: parsed.data,
        columns: columns,
        paging: true,
        searching: false,
        info: false,
        lengthChange: false,
        pageLength: 10,
        order: [[0, "asc"]],
      });
    })
    .fail(function () {
      $("#csvTable").html(
        "<tr><td colspan='10'>Failed to load CSV results.</td></tr>"
      );
    });
});
