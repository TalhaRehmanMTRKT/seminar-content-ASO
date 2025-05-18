document.addEventListener("DOMContentLoaded", () => {
  // Load main results table
  const mainCSVPath = 'code/results.csv';
  fetch(mainCSVPath)
    .then(response => {
      if (!response.ok) throw new Error(`Failed to fetch ${mainCSVPath}`);
      return response.text();
    })
    .then(csvText => {
      const parsed = Papa.parse(csvText, {
        header: true,
        skipEmptyLines: true,
        dynamicTyping: true
      });

      if (parsed.errors.length) {
        console.error("CSV parsing errors in results.csv:", parsed.errors);
        document.getElementById('csvTable').innerHTML =
          '<tr><td colspan="10">Error parsing CSV file.</td></tr>';
        return;
      }

      const data = parsed.data;
      const columns = parsed.meta.fields.map(field => ({ title: field, data: field }));

      $('#csvTable').DataTable({
        data: data,
        columns: columns,
        pageLength: 10,
        lengthMenu: [5, 10, 25, 50],
        responsive: true
      });
    })
    .catch(error => {
      console.error('Error loading results.csv:', error);
      document.getElementById('csvTable').innerHTML =
        '<tr><td colspan="10">Failed to load CSV file.</td></tr>';
    });

  // Load theta and power flow CSVs
  const csvFiles = [
    { path: "code/theta.csv", tableId: "thetaTable" },
    { path: "code/powerflow.csv", tableId: "powerflowTable" }
  ];

  csvFiles.forEach(({ path, tableId }) => {
    fetch(path)
      .then(response => {
        if (!response.ok) throw new Error(`Failed to fetch ${path}`);
        return response.text();
      })
      .then(csvText => {
        const parsed = Papa.parse(csvText, {
          header: true,
          skipEmptyLines: true,
          dynamicTyping: true
        });

        if (parsed.errors.length) {
          console.error(`CSV parsing errors in ${path}:`, parsed.errors);
          document.getElementById(tableId).innerHTML =
            '<tr><td colspan="10">Parsing Error</td></tr>';
          return;
        }

        const data = parsed.data;
        const columns = parsed.meta.fields.map(field => ({
          title: field,
          data: field
        }));

        $(`#${tableId}`).DataTable({
          data: data,
          columns: columns,
          pageLength: 10,
          lengthMenu: [5, 10, 25, 50],
          responsive: true
        });
      })
      .catch(error => {
        console.error(`Error loading ${path}:`, error);
        document.getElementById(tableId).innerHTML =
          '<tr><td colspan="10">Failed to load CSV file.</td></tr>';
      });
  });

  // Tab switching logic
  document.querySelectorAll(".tab-btn").forEach(button => {
    button.addEventListener("click", () => {
      document.querySelectorAll(".tab-btn").forEach(btn => btn.classList.remove("active"));
      document.querySelectorAll(".tab-content").forEach(tab => tab.classList.remove("active"));

      button.classList.add("active");
      document.getElementById(button.dataset.tab).classList.add("active");
    });
  });
});
