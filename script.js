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

  // Load theta and powerflow CSVs
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

  // Powerflow heatmap logic
  const canvas = document.getElementById("powerFlowHeatmap");
  const hourSelect = document.getElementById("hourSelect");
  const busLabels = ['Bus 1', 'Bus 2', 'Bus 3', 'Bus 4'];
  let heatmapData = [];

  fetch("code/powerflow.csv")
    .then(res => res.text())
    .then(csvText => {
      const parsed = Papa.parse(csvText, {
        header: true,
        skipEmptyLines: true,
        dynamicTyping: true
      });

      heatmapData = parsed.data;

      // Populate dropdown
      heatmapData.forEach((row, i) => {
        const opt = document.createElement("option");
        opt.value = i;
        opt.text = `Hour ${row.Hour}`;
        hourSelect.appendChild(opt);
      });

      plotHeatmap(0);

      hourSelect.addEventListener("change", () => {
        plotHeatmap(parseInt(hourSelect.value));
      });
    });

  let heatmapChart;
  function plotHeatmap(hourIndex) {
    const row = heatmapData[hourIndex];
    const matrix = [];

    for (let i = 1; i <= 4; i++) {
      for (let j = 1; j <= 4; j++) {
        const val = row[`P${i}${j}`];
        matrix.push({
          x: j - 1,
          y: i - 1,
          v: val
        });
      }
    }

    const dataset = {
      label: 'Power Flow',
      data: matrix,
      borderColor: 'black', // Thick black borders
      borderWidth: 1.5,
      backgroundColor(context) {
        const v = context.dataset.data[context.dataIndex].v;
        const alpha = Math.min(1, Math.abs(v) / 70);
        return v >= 0
          ? `rgba(0, 200, 0, ${alpha})` // strong green
          : `rgba(200, 0, 0, ${alpha})`; // strong red
      },
      width: ({ chart }) => (chart.chartArea || {}).width / 4 - 1,
      height: ({ chart }) => (chart.chartArea || {}).height / 4 - 1
    };

    const config = {
      type: 'matrix',
      data: { datasets: [dataset] },
      options: {
        maintainAspectRatio: true,
        aspectRatio: 1,
        responsive: true,
        layout: {
          padding: 10
        },
        plugins: {
          tooltip: {
            backgroundColor: '#222',
            titleColor: '#fff',
            bodyColor: '#ddd',
            callbacks: {
              title: ctx => {
                const x = ctx[0].raw.x;
                const y = ctx[0].raw.y;
                return `From ${busLabels[y]} to ${busLabels[x]}`;
              },
              label: ctx => `Power Flow: ${ctx.raw.v.toFixed(2)}`
            }
          },
          title: {
            display: true,
            text: `Power Flow Heatmap (Hour ${heatmapData[hourIndex].Hour})`,
            color: '#111',
            font: {
              size: 18,
              weight: 'bold'
            },
            padding: {
              top: 10,
              bottom: 10
            }
          },
          legend: { display: false }
        },
        scales: {
          x: {
            type: 'linear',
            position: 'top',
            min: -0.5,
            max: 3.5,
            ticks: {
              stepSize: 1,
              callback: (v) => busLabels[v],
              color: '#111',
              font: { size: 13, weight: '600' }
            },
            grid: {
              display: false
            }
          },
          y: {
            type: 'linear',
            reverse: true,
            min: -0.5,
            max: 3.5,
            ticks: {
              stepSize: 1,
              callback: (v) => busLabels[v],
              color: '#111',
              font: { size: 13, weight: '600' }
            },
            grid: {
              display: false
            }
          }
        }
      }
    };

    if (heatmapChart) heatmapChart.destroy();
    heatmapChart = new Chart(canvas, config);
  }


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
