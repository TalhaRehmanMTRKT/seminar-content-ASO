document.addEventListener("DOMContentLoaded", () => {
  const csvFilePath = 'code/results.csv'; // adjust this path as needed

  fetch(csvFilePath)
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }
      return response.text();
    })
    .then(csvText => {
      const parsed = Papa.parse(csvText, {
        header: true,
        skipEmptyLines: true,
        dynamicTyping: true
      });

      if (parsed.errors.length) {
        console.error("CSV parsing errors:", parsed.errors);
        document.getElementById('csvTable').innerHTML =
          '<tr><td colspan="10">Error parsing CSV file.</td></tr>';
        return;
      }

      const data = parsed.data;
      const columns = parsed.meta.fields.map(field => ({
        title: field,
        data: field
      }));

      $('#csvTable').DataTable({
        data: data,
        columns: columns,
        pageLength: 10,
        lengthMenu: [5, 10, 25, 50],
        responsive: true
      });
    })
    .catch(error => {
      console.error('Error loading CSV:', error);
      document.getElementById('csvTable').innerHTML =
        '<tr><td colspan="10">Failed to load CSV file.</td></tr>';
    });
});
