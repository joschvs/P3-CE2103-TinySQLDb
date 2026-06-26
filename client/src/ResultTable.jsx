export default function ResultTable({ columns, rows }) {
    return (
        <table className="result-table">
            <thead>
            <tr>
                {columns.map((col, idx) => (
                    <th key={idx}>{col}</th>
                ))}
            </tr>
            </thead>
            <tbody>
            {rows.map((row, rowIdx) => (
                <tr key={rowIdx}>
                    {row.map((cell, cellIdx) => (
                        <td key={cellIdx}>{cell}</td>
                    ))}
                </tr>
            ))}
            </tbody>
        </table>
    );
}